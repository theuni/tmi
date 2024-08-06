// Copyright (c) 2024 Cory Fields
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TMI_HASHER_H_
#define TMI_HASHER_H_

#include "tminode.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <tuple>
#include <vector>

namespace tmi {

template <typename T, int ComparatorSize, int NodeSize>
struct hasher_insert_hints {
    using node_type = tminode<T, ComparatorSize, NodeSize>;
    using base_type = node_type::base_type;
    size_t m_hash{0};
    base_type** m_bucket{nullptr};
};

template <typename T, int ComparatorSize, int NodeSize>
struct hasher_premodify_cache {
    using node_type = tminode<T, ComparatorSize, NodeSize>;
    using base_type = node_type::base_type;
    bool m_is_head{false};
    base_type** m_bucket{nullptr};
    base_type* m_prev{nullptr};
};

template <typename T, int ComparatorSize, int NodeSize>
struct tmi_hasher_base{};


template <typename T, int ComparatorSize, int NodeSize, int I, typename Hasher, typename Parent>
class tmi_hasher : public tmi_hasher_base<T, ComparatorSize, NodeSize>
{
    using node_type = tminode<T, ComparatorSize, NodeSize>;
    using base_type = node_type::base_type;
    using hash_buckets = std::vector<base_type*>;
    using insert_hints_type = hasher_insert_hints<T, ComparatorSize, NodeSize>;
    using premodify_cache = hasher_premodify_cache<T, ComparatorSize, NodeSize>;
    using hash_type = Hasher::hash_type;

    friend Parent;

    static constexpr size_t first_hashes_resize = 2048;

    Parent& m_parent;
    Hasher m_hasher;
    hash_buckets m_buckets;
    size_t m_size = 0;

    tmi_hasher(Parent& parent) : m_parent(parent){}

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
                size_t index = cur_node->template hash<I>() % new_buckets.size();
                base_type*& new_bucket = new_buckets.at(index);
                cur_node->template set_next_hashptr<I>(new_bucket);
                new_bucket = cur_node;
                cur_node = next_node;
            }
        }
        m_buckets = std::move(new_buckets);
    }

    void hash_remove_direct(base_type* node)
    {
        auto hash = node->template hash<I>();
        size_t bucket_count = m_buckets.size();
        if (!bucket_count) {
            return;
        }
        auto index = hash % bucket_count;

        base_type*& bucket = m_buckets.at(index);
        base_type* cur_node = bucket;
        base_type* prev_node = cur_node;
        while (cur_node) {
            if (cur_node->template hash<I>() == hash && m_hasher(cur_node->node()->value(), node->node()->value())) {
                if (cur_node == prev_node) {
                    // head of list
                    bucket = cur_node->template next_hash<I>();
                } else {
                    prev_node->template set_next_hashptr<I>(cur_node->template next_hash<I>());
                }
                m_size--;
                break;
            }
            prev_node = cur_node;
            cur_node = cur_node->template next_hash<I>();
        }
    }

    /*
        First rehash if necessary, using first_hashes_resize as the initial
        size if empty. Then find calculate the bucket and insert there.
    */
    bool preinsert_node_hash(node_type* node, insert_hints_type& hints)
    {
        size_t bucket_count = m_buckets.size();
        auto hash = m_hasher(node->value());

        if (!bucket_count) {
            m_buckets.resize(first_hashes_resize, nullptr);
            bucket_count = first_hashes_resize;
        } else if (static_cast<double>(m_size) / static_cast<double>(bucket_count) >= 0.8) {
            bucket_count *= 2;
            rehash(bucket_count);
        }

        size_t index = hash % m_buckets.size();
        base_type*& bucket = m_buckets.at(index);

        if constexpr (Hasher::hashed_unique()) {
            base_type* curr = bucket;
            base_type* prev = curr;
            while (curr) {
                if (m_hasher(curr->node()->value(), node->value())) {
                    return false;
                }
                prev = curr;
                curr = curr->template next_hash<I>();
            }
        }
        hints.m_bucket = &bucket;
        hints.m_hash = hash;
        return true;
    }

    void hasher_create_premodify_cache(node_type* node, premodify_cache& cache)
    {
        base_type* base = node->get_base();
        auto hash = base->template hash<I>();
        size_t bucket_count = m_buckets.size();
        if (!bucket_count) {
            return;
        }
        auto index = hash % bucket_count;

        base_type*& bucket = m_buckets.at(index);
        base_type* cur_node = bucket;
        base_type* prev_node = cur_node;
        while (cur_node) {
            if (cur_node->template hash<I>() == hash && m_hasher(cur_node->node()->value(), node->value())) {
                if (cur_node == prev_node) {
                    cache.m_bucket = &bucket;
                    cache.m_is_head = true;
                } else {
                    cache.m_prev = prev_node;
                    cache.m_is_head = false;
                }
                break;
            }
            prev_node = cur_node;
            cur_node = cur_node->template next_hash<I>();
        }
    }

    bool hasher_erase_if_modified(node_type* node, const premodify_cache& cache)
    {
        base_type* base = node->get_base();
        if (m_hasher(node->value()) != base->template hash<I>()) {
            if (cache.m_is_head) {
                *cache.m_bucket = nullptr;
            } else {
                cache.m_prev->template set_next_hashptr<I>(base->template next_hash<I>());
            }
            m_size--;
            return true;
        }
        return false;
    }

    void insert_node_hash(node_type* node, const insert_hints_type& hints)
    {
        base_type* node_base = node->get_base();
        node_base->template set_hash<I>(hints.m_hash);
        node_base->template set_next_hashptr<I>(*hints.m_bucket);
        *hints.m_bucket = node_base;
        m_size++;
    }


    node_type* find_hash(const hash_type& hash_key) const
    {
        size_t hash = m_hasher(hash_key);
        size_t bucket_count = m_buckets.size();
        if (!bucket_count) {
            return nullptr;
        }
        auto* node = m_buckets.at(hash % bucket_count);
        while (node) {
            if (node->template hash<I>() == hash) {
                if (m_hasher(node->node()->value(), hash_key)) {
                    return node->node();
                }
            }
            node = node->template next_hash<I>();
        }
        return nullptr;
    }

    void clear()
    {
        m_buckets.clear();
    }
public:

    class iterator
    {
        node_type* m_node{};
        const hash_buckets* m_buckets{nullptr};
        size_t m_bucket{0};

        iterator(node_type* node, const hash_buckets* buckets, size_t bucket) : m_node(node), m_buckets(buckets), m_bucket(bucket) {}
        friend class tmi_hasher;
    public:
        typedef T value_type;
        typedef T* pointer;
        typedef T& reference;
        using element_type = T;
        iterator() = default;
        T& operator*() const { return m_node->value(); }
        T* operator->() const { return &m_node->value(); }
        iterator& operator++()
        {
            base_type* next = get_next_hash(m_node->get_base());
            while (next == nullptr) {
                next = m_buckets->at(m_bucket++);
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
            iterator copy(m_node, m_buckets, m_bucket);
            ++(*this);
            return copy;
        }
        bool operator==(iterator rhs) const { return m_node == rhs.m_node; }
        bool operator!=(iterator rhs) const { return m_node != rhs.m_node; }
    };

    class const_iterator
    {
        //TODO: Make const
        node_type* m_node{};

        const hash_buckets* m_buckets{nullptr};
        size_t m_bucket{0};

        const_iterator(node_type* node, const hash_buckets* buckets, size_t bucket) : m_node(node), m_buckets(buckets), m_bucket(bucket) {}
        friend class tmi_hasher;
    public:
        typedef const T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        using element_type = const T;
        const_iterator() = default;
        const T& operator*() const { return m_node->value(); }
        const T* operator->() const { return &m_node->value(); }
        const_iterator& operator++()
        {
            base_type* next = get_next_hash(m_node->get_base());
            while (next == nullptr) {
                next = m_buckets->at(m_bucket++);
            }
            if (next == nullptr) {
                m_node = nullptr;
            } else {
                m_node = next->node();
            }
            return *this;
        }
        const_iterator operator++(int)
        {
            const_iterator copy(m_node, m_buckets, m_bucket);
            ++(*this);
            return copy;
        }
        bool operator==(const_iterator rhs) const { return m_node == rhs.m_node; }
        bool operator!=(const_iterator rhs) const { return m_node != rhs.m_node; }
    };

    iterator begin()
    {
        for (size_t i = 0; i < m_buckets.size(); ++i) {
            base_type* base = m_buckets.at(i);
            if (base) {
                return iterator(base->node(), &m_buckets, i);
            }
        }
        return iterator(nullptr, &m_buckets, 0);
    }

    const_iterator begin() const
    {
        for (size_t i = 0; i < m_buckets.size(); ++i) {
            base_type* base = m_buckets.at(i);
            if (base) {
                return const_iterator(base->node(), &m_buckets, i);
            }
        }
        return const_iterator(nullptr, m_buckets, 0);
    }

    const_iterator end() const
    {
        return const_iterator(nullptr, m_buckets, 0);
    }

    iterator end()
    {
        return iterator(nullptr, m_buckets, 0);
    }

};

} // namespace tmi

#endif // TMI_HASHER_H_
