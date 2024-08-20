// Copyright (c) 2024 Cory Fields
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TMI_HASHER_H_
#define TMI_HASHER_H_

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <tuple>
#include <utility>
#include <vector>

namespace tmi {

template <typename T, typename Node, typename Hasher, typename Parent, int I>
class tmi_hasher
{
    using node_type = Node;
    using base_type = typename node_type::base_type;
    using hash_buckets = std::vector<base_type*>;
    using size_type = size_t;
    using key_from_value = typename Hasher::key_from_value_type;
    using key_type = typename key_from_value::result_type;
    using hasher = typename Hasher::hasher_type;
    using key_equal = typename Hasher::pred_type;
    using ctor_args = std::tuple<size_type,key_from_value,hasher,key_equal>;
    friend Parent;

    static constexpr bool hashed_unique() { return Hasher::is_hashed_unique(); }

    struct insert_hints {
        size_t m_hash{0};
        base_type** m_bucket{nullptr};
    };

    struct premodify_cache {
        base_type** m_bucket{nullptr};
        base_type* m_prev{nullptr};
    };

    static constexpr bool requires_premodify_cache() { return true; }

    static constexpr size_t first_hashes_resize = 2048;

    Parent& m_parent;
    hash_buckets m_buckets;
    key_from_value m_key_from_value;
    hasher m_hasher;
    key_equal m_pred;

    tmi_hasher(Parent& parent) : m_parent(parent){}

    tmi_hasher(Parent& parent, const ctor_args& args) : m_parent(parent), m_buckets(std::get<0>(args), nullptr), m_key_from_value(std::get<1>(args)), m_hasher(std::get<2>(args)), m_pred(std::get<3>(args)){}

    tmi_hasher(Parent& parent, const tmi_hasher& rhs) : m_parent(parent), m_buckets(rhs.m_buckets.size(), nullptr), m_key_from_value(rhs.m_key_from_value), m_hasher(rhs.m_hasher), m_pred(rhs.m_pred){}
    tmi_hasher(Parent& parent, tmi_hasher&& rhs) : m_parent(parent), m_buckets(std::move(rhs.m_buckets)), m_key_from_value(std::move(rhs.m_key_from_value)), m_hasher(std::move(rhs.m_hasher)), m_pred(std::move(rhs.m_pred))
    {
        rhs.m_buckets.clear();
    }

    static base_type* get_next_hash(base_type* base)
    {
        return base->template next_hash<I>();
    }

    static size_t get_hash(base_type* base)
    {
        return base->template hash<I>();
    }

    static void set_hash(base_type* base, size_t hash)
    {
        base->template set_hash<I>(hash);
    }

    static void set_next_hashptr(base_type* lhs, base_type* rhs)
    {
        lhs->template set_next_hashptr<I>(rhs);
    }

    /*
        Create new buckets, iterate through the old ones moving them to
        their updated indicies in the new buckets, then replace the old
        with the new
    */
    void rehash(size_t new_size)
    {
        hash_buckets new_buckets(new_size, nullptr);
        for (base_type* bucket : m_buckets) {
            base_type* cur_node = bucket;
            while (cur_node) {
                base_type* next_node = cur_node->template next_hash<I>();
                const size_t index = cur_node->template hash<I>() % new_size;
                base_type*& new_bucket = new_buckets.at(index);
                cur_node->template set_next_hashptr<I>(new_bucket);
                new_bucket = cur_node;
                cur_node = next_node;
            }
        }
        m_buckets = std::move(new_buckets);
    }

    void remove_node(const node_type* node)
    {
        const base_type* base = node->get_base();
        const size_t bucket_count = m_buckets.size();
        if (!bucket_count) {
            return;
        }
        const size_t index = base->template hash<I>() % bucket_count;

        base_type*& bucket = m_buckets.at(index);
        base_type* cur_node = bucket;
        base_type* prev_node = cur_node;
        while (cur_node) {
            if (cur_node->node() == node) {
                if (cur_node == prev_node) {
                    // head of list
                    bucket = cur_node->template next_hash<I>();
                } else {
                    prev_node->template set_next_hashptr<I>(cur_node->template next_hash<I>());
                }
                break;
            }
            prev_node = cur_node;
            cur_node = cur_node->template next_hash<I>();
        }
    }

    void insert_node_direct(node_type* node)
    {
        base_type* base = node->get_base();

        if (base->template hash<I>() == 0) {
            const size_t hash = m_hasher(m_key_from_value(node->value()));
            base->template set_hash<I>(hash);
        }
        const size_t index = base->template hash<I>() % m_buckets.size();
        base_type*& bucket = m_buckets.at(index);

        base->template set_next_hashptr<I>(bucket);
        bucket = base;
    }

    /*
        First rehash if necessary, using first_hashes_resize as the initial
        size if empty. Then find calculate the bucket and insert there.
    */
    node_type* preinsert_node(const node_type* node, insert_hints& hints)
    {
        size_t bucket_count = m_buckets.size();
        const auto& key = m_key_from_value(node->value());
        const size_t hash = m_hasher(key);

        if (!bucket_count) {
            m_buckets.resize(first_hashes_resize, nullptr);
            bucket_count = first_hashes_resize;
        } else if (static_cast<double>(m_parent.get_size()) / static_cast<double>(bucket_count) >= 0.8) {
            bucket_count *= 2;
            rehash(bucket_count);
        }

        const size_t index = hash % m_buckets.size();
        base_type*& bucket = m_buckets.at(index);

        if constexpr (hashed_unique()) {
            base_type* curr = bucket;
            base_type* prev = curr;
            while (curr) {
                if (curr->template hash<I>() == hash) {
                    if (m_pred(m_key_from_value(curr->node()->value()), key)) {
                        return curr->node();
                    }
                }
                prev = curr;
                curr = curr->template next_hash<I>();
            }
        }
        hints.m_bucket = &bucket;
        hints.m_hash = hash;
        return nullptr;
    }

    void create_premodify_cache(const node_type* node, premodify_cache& cache)
    {
        const base_type* base = node->get_base();
        const size_t bucket_count = m_buckets.size();
        if (!bucket_count) {
            return;
        }
        const size_t index = base->template hash<I>() % bucket_count;

        base_type*& bucket = m_buckets.at(index);
        base_type* cur_node = bucket;
        base_type* prev_node = cur_node;
        while (cur_node) {
            if (cur_node->node() == node) {
                if (cur_node == prev_node) {
                    cache.m_prev = nullptr;
                    cache.m_bucket = &bucket;
                } else {
                    cache.m_prev = prev_node;
                    cache.m_bucket = nullptr;
                }
                break;
            }
            prev_node = cur_node;
            cur_node = cur_node->template next_hash<I>();
        }
    }

    bool erase_if_modified(const node_type* node, const premodify_cache& cache)
    {
        const base_type* base = node->get_base();
        if (m_hasher(m_key_from_value(node->value())) != base->template hash<I>()) {
            if (cache.m_prev) {
                cache.m_prev->template set_next_hashptr<I>(base->template next_hash<I>());
            } else {
                *cache.m_bucket = base->template next_hash<I>();
            }
            return true;
        }
        return false;
    }

    void insert_node(node_type* node, const insert_hints& hints)
    {
        base_type* node_base = node->get_base();
        node_base->template set_hash<I>(hints.m_hash);
        node_base->template set_next_hashptr<I>(*hints.m_bucket);
        *hints.m_bucket = node_base;
    }


    node_type* find_hash(const key_type& hash_key) const
    {
        const size_t hash = m_hasher(hash_key);
        const size_t bucket_count = m_buckets.size();
        if (!bucket_count) {
            return nullptr;
        }
        auto* node = m_buckets.at(hash % bucket_count);
        while (node) {
            if (node->template hash<I>() == hash) {
                if (m_pred(m_key_from_value(node->node()->value()), hash_key)) {
                    return node->node();
                }
            }
            node = node->template next_hash<I>();
        }
        return nullptr;
    }

    void do_clear()
    {
        m_buckets.clear();
    }

public:

    class iterator
    {
        const node_type* m_node{};
        const hash_buckets* m_buckets{nullptr};

        iterator(const node_type* node, const hash_buckets* buckets) : m_node(node), m_buckets(buckets) {}
        friend class tmi_hasher;
    public:

        typedef const T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        using difference_type = std::ptrdiff_t;
        using element_type = const T;
        using iterator_category = std::forward_iterator_tag;
        iterator() = default;
        const T& operator*() const { return m_node->value(); }
        const T* operator->() const { return &m_node->value(); }
        iterator& operator++()
        {
            const base_type* next = m_node->get_base()->template next_hash<I>();
            if (!next) {
                const size_t bucket_size = m_buckets->size();
                size_t bucket = m_node->get_base()->template hash<I>() % bucket_size;
                bucket++;
                for (; bucket < bucket_size; ++bucket) {
                    next = m_buckets->at(bucket);
                    if (next) {
                        break;
                    }
                }
            }
            if (next == nullptr) {
                m_node = nullptr;
            } else {
                m_node = next->node();
            }
            return *this;
        }
        iterator operator++(int)
        {
            iterator copy(m_node, m_buckets);
            ++(*this);
            return copy;
        }
        bool operator==(iterator rhs) const { return m_node == rhs.m_node; }
        bool operator!=(iterator rhs) const { return m_node != rhs.m_node; }
    }
;
    using const_iterator = iterator;

    iterator begin()
    {
        for (const auto& bucket : m_buckets) {
            if (bucket) {
                return make_iterator(bucket->node());
            }
        }
        return end();
    }

    const_iterator begin() const
    {
        for (const auto& bucket : m_buckets) {
            if (bucket) {
                return make_iterator(bucket->node());
            }
        }
        return end();
    }

    iterator end()
    {
        return make_iterator(nullptr);
    }

    const_iterator end() const
    {
        return make_iterator(nullptr);
    }

    iterator iterator_to(const T& entry)
    {
        const node_type* node = reinterpret_cast<const node_type*>(&entry);
        return make_iterator(node);
    }

    const_iterator iterator_to(const T& entry) const
    {
        const node_type* node = reinterpret_cast<const node_type*>(&entry);
        return make_iterator(node);
    }

    template <typename... Args>
    std::pair<iterator,bool> emplace(Args&&... args)
    {
        auto [node, success] = m_parent.do_emplace(std::forward<Args>(args)...);
        return std::make_pair(make_iterator(node), success);
    }

    template <typename Callable>
    bool modify(iterator it, Callable&& func)
    {
        node_type* node = const_cast<node_type*>(it.m_node);
        if (!node) return false;
        return m_parent.do_modify(node, std::forward<Callable>(func));
    }

    iterator find(const key_type& hash_key) const
    {
        const size_t hash = m_hasher(hash_key);
        const size_t bucket_count = m_buckets.size();
        if (!bucket_count) {
            return end();
        }
        auto* node = m_buckets.at(hash % bucket_count);
        while (node) {
            if (node->template hash<I>() == hash) {
                if (m_pred(m_key_from_value(node->node()->value()), hash_key)) {
                    return make_iterator(node->node());
                }
            }
            node = node->template next_hash<I>();
        }
        return end();
    }

    iterator find(const T& value) const
    {
        const auto& key = m_key_from_value(value);
        const size_t hash = m_hasher(key);
        const size_t bucket_count = m_buckets.size();
        if (!bucket_count) {
            return end();
        }
        auto* node = m_buckets.at(hash % bucket_count);
        while (node) {
            if (node->template hash<I>() == hash) {
                if (m_pred(m_key_from_value(node->node()->value()), key)) {
                    return make_iterator(node->node());
                }
            }
            node = node->template next_hash<I>();
        }
        return end();
    }

    iterator erase(iterator it)
    {
        node_type* node = const_cast<node_type*>(it.m_node);
        if (!node) return end();
        base_type* next = get_next_hash(node->get_base());
        if (!next) {
            const size_t bucket_size = m_buckets.size();
            size_t bucket = node->get_base()->template hash<I>() % bucket_size;
            bucket++;
            for (; bucket < bucket_size; ++bucket) {
                next = m_buckets.at(bucket);
                if (next != nullptr) break;
            }
        }
        m_parent.do_erase(node);
        if (!next) {
            return end();
        }
        return make_iterator(next->node());
    }

    size_t count(const T& value) const
    {
        size_t ret = 0;
        const auto& key = m_key_from_value(value);
        const size_t hash = m_hasher(key);
        const size_t bucket_count = m_buckets.size();
        if (!bucket_count) {
            return 0;
        }
        auto* node = m_buckets.at(hash % bucket_count);
        while (node) {
            if (node->template hash<I>() == hash) {
                if (m_pred(m_key_from_value(node->node()->value()), key)) {
                    ret++;
                    if constexpr (hashed_unique()) break;
                }
            }
            node = node->template next_hash<I>();
        }
        return ret;
    }

    size_t count(const key_type& hash_key) const
    {
        size_t ret = 0;
        const size_t hash = m_hasher(hash_key);
        const size_t bucket_count = m_buckets.size();
        if (!bucket_count) {
            return 0;
        }
        auto* node = m_buckets.at(hash % bucket_count);
        while (node) {
            if (node->template hash<I>() == hash) {
                if (m_pred(m_key_from_value(node->node()->value()), hash_key)) {
                    ret++;
                    if constexpr (hashed_unique()) break;
                }
            }
            node = node->template next_hash<I>();
        }
        return ret;
    }

    void clear()
    {
        m_parent.do_clear();
    }

    size_t size() const
    {
        return m_parent.get_size();
    }

    bool empty() const
    {
        return m_parent.get_empty();
    }

private:

    const node_type* node_from_iterator(iterator it) const
    {
        return it.m_node;
    }

    iterator make_iterator(const node_type* node) const
    {
        return iterator(node, &m_buckets);
    }
};

} // namespace tmi

#endif // TMI_HASHER_H_
