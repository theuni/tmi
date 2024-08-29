// Copyright (c) 2024 Cory Fields
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Parts of this file have been copied from LLVM's libc++ and modified
// to work here. Their license applies to those functions. They are intended
// to serve only as a proof of concept for the red-black tree and should be
// rewritten asap.

#ifndef TMI_COMPARATOR_H_
#define TMI_COMPARATOR_H_

#include "tmi_nodehandle.h"

#include <cassert>
#include <cstddef>
#include <iterator>
#include <utility>
#include <tuple>

namespace tmi {

template <typename T, typename Node, typename Comparator, typename Parent, typename Allocator, int I>
class tmi_comparator
{
public:
    class iterator;

    using node_type = Node;
    using key_from_value = typename Comparator::key_from_value_type;
    using key_compare = typename Comparator::comparator;
    using key_type = typename key_from_value::result_type;
    using ctor_args = std::tuple<key_from_value,key_compare>;
    using allocator_type = Allocator;
    using node_allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<node_type>;
    using node_handle = detail::node_handle<Allocator, Node>;
    using insert_return_type = detail::insert_return_type<iterator, node_handle>;

private:
    static constexpr bool sorted_unique() { return Comparator::is_ordered_unique(); }
    friend Parent;

    struct insert_hints {
        node_type* m_parent{nullptr};
        bool m_inserted_left{false};
    };

    struct premodify_cache{};
    static constexpr bool requires_premodify_cache() { return false; }

    Parent& m_parent;

    node_type* m_root{nullptr};
    key_from_value m_key_from_value;
    key_compare m_comparator;

    tmi_comparator(Parent& parent, const allocator_type&) : m_parent(parent){}

    tmi_comparator(Parent& parent, const allocator_type&, const ctor_args& args) : m_parent(parent), m_key_from_value(std::get<0>(args)), m_comparator(std::get<1>(args)){}
    tmi_comparator(Parent& parent, const tmi_comparator& rhs) : m_parent(parent), m_key_from_value(rhs.m_key_from_value), m_comparator(rhs.m_comparator){}
    tmi_comparator(Parent& parent, tmi_comparator&& rhs) : m_parent(parent), m_root(rhs.m_root), m_key_from_value(std::move(rhs.m_key_from_value)), m_comparator(std::move(rhs.m_comparator))
    {
        rhs.m_root = nullptr;
    }

    static void set_right(node_type* lhs, node_type* rhs)
    {
        lhs->template set_right<I>(rhs);
    }

    static void set_left(node_type* lhs, node_type* rhs)
    {
        lhs->template set_left<I>(rhs);
    }

    static node_type* get_parent(node_type* node)
    {
        return node->template parent<I>();
    }

    static void set_parent(node_type* lhs, node_type* rhs)
    {
        lhs->template set_parent<I>(rhs);
    }

    static int get_bf(node_type* node)
    {
        return node->template bf<I>();
    }

    static void set_bf(node_type* node, int bf)
    {
        node->template set_bf<I>(bf);
    }

    static node_type* get_left(node_type* node)
    {
        return node->template left<I>();
    }

    static node_type* get_right(node_type* node)
    {
        return node->template right<I>();
    }

    static bool tree_is_left_child(node_type* node)
    {
        return node == get_left(get_parent(node));
    }

    static bool tree_is_left_child(const node_type* node)
    {
        return node == get_left(get_parent(node));
    }

    static node_type* tree_min(node_type* node)
    {
        assert(node);
        do {
            node = get_left(node);
        } while(node);
        return node;
    }

    static const node_type* tree_min(const node_type* node)
    {
        assert(node);
        do {
            node = get_left(node);
        } while(node);
        return node;
    }

    static node_type* tree_max(node_type* node) {
        assert(node);
        do {
            node = get_right(node);
        } while(node);
        return node;
    }

    static const node_type* tree_max(const node_type* node) {
        assert(node);
        do {
            node = get_right(node);
        } while(node);
        return node;
    }

    static node_type* tree_next(node_type* node)
    {
        assert(node);
        if (get_right(node)) {
            return tree_min(get_right(node));
        }
        while (node && !tree_is_left_child(node)) {
            node = get_parent(node);
        }
        return node ? get_parent(node) : nullptr;
    }

    static const node_type* tree_next(const node_type* node)
    {
        assert(node);
        if (get_right(node)) {
            return tree_min(get_right(node));
        }
        while (node && !tree_is_left_child(node)) {
            node = get_parent(node);
        }
        return node ? get_parent(node) : nullptr;
    }

    static node_type* tree_prev(node_type* node) {
        assert(node);
        if (get_left(node)) {
            return tree_max(get_left(node));
        }
        while (node && tree_is_left_child(node)) {
            node = get_parent(node);
        }
        return node ? get_parent(node) : nullptr;
    }

    static const node_type* tree_prev(const node_type* node) {
        assert(node);
        if (get_left(node)) {
            return tree_max(get_left(node));
        }
        while (node && tree_is_left_child(node)) {
            node = get_parent(node);
        }
        return node ? get_parent(node) : nullptr;
    }

    node_type* tree_remove(node_type* node)
    {
        //TODO:
        node_type* new_root = nullptr;
        return new_root;
    }

    node_type* tree_balance_after_insert(node_type* root, node_type* node)
    {
        //TODO:
        node_type* new_root = nullptr;
        return new_root;
    }

    void remove_node(node_type* node)
    {
        m_root = tree_remove(node);
    }

    void insert_node_direct(node_type* node)
    {
        node_type* parent = nullptr;
        node_type* curr = m_root;
        const auto& key = m_key_from_value(node->value());

        bool inserted_left = false;
        while (curr != nullptr) {
            parent = curr;
            const auto& curr_key = m_key_from_value(curr->value());
            if (m_comparator(key, curr_key)) {
                curr = curr->template left<I>();
                inserted_left = true;
            } else {
                curr = curr->template right<I>();
                inserted_left = false;
            }
        }

        node->template set_left<I>(nullptr);
        node->template set_right<I>(nullptr);
        node->template set_bf<I>(0);
        node->template set_parent<I>(parent);

        if(parent) {
            if (inserted_left) {
                parent->template set_left<I>(node);
            } else {
                parent->template set_right<I>(node);
            }
            m_root = tree_balance_after_insert(m_root, node);
        } else {
            m_root = node;
        }
    }

    node_type* preinsert_node(const node_type* node, insert_hints& hints)
    {
        node_type* parent = nullptr;
        node_type* curr = m_root;
        const auto& key = m_key_from_value(node->value());

        bool inserted_left = false;
        while (curr != nullptr) {
            parent = curr;
            const auto& curr_key = m_key_from_value(curr->value());
            if constexpr (sorted_unique()) {
                if (m_comparator(key, curr_key)) {
                    curr = curr->template left<I>();
                    inserted_left = true;
                } else if (m_comparator(curr_key, key)) {
                    curr = curr->template right<I>();
                    inserted_left = false;
                } else {
                    return curr;
                }
            } else {
                if (m_comparator(key, curr_key)) {
                    curr = curr->template left<I>();
                    inserted_left = true;
                } else {
                    curr = curr->template right<I>();
                    inserted_left = false;
                }
            }
        }
        hints.m_inserted_left = inserted_left;
        hints.m_parent = parent;
        return nullptr;
    }

    void insert_node(node_type* node, const insert_hints& hints)
    {
        node_type* parent = hints.m_parent;

        node->template set_left<I>(nullptr);
        node->template set_right<I>(nullptr);
        node->template set_bf<I>(0);
        node->template set_parent<I>(parent);

        if (!parent) {
            m_root = node;
        } else {
            if (hints.m_inserted_left) {
                parent->template set_left<I>(node);
            } else {
                parent->template set_right<I>(node);
            }
            m_root = tree_balance_after_insert(m_root, node);
        }
    }

    bool erase_if_modified(node_type* node, const premodify_cache&)
    {
        node_type* next_ptr = nullptr;
        node_type* prev_ptr = nullptr;

        if (node != tree_min(m_root))
            prev_ptr = tree_prev(node);
        if (node != tree_max(m_root))
            next_ptr = tree_next(node);

        const auto& key = m_key_from_value(node->value());

        bool needs_resort = ((next_ptr != nullptr && m_comparator(m_key_from_value(next_ptr->value()), key)) ||
                             (prev_ptr != nullptr && m_comparator(key, m_key_from_value(prev_ptr->value()))));
        if (needs_resort) {
            m_root = tree_remove(node);
            node->template set_parent<I>(nullptr);
            node->template set_left<I>(nullptr);
            node->template set_right<I>(nullptr);
            node->template set_bf<I>(0);
            return true;
        }
        return false;
    }

    void do_clear()
    {
        m_root = nullptr;
    }

public:

    class iterator
    {
        const node_type* m_node{};
        const node_type* const* m_root{};
        iterator(const node_type* node, const node_type* const* root) : m_node(node), m_root(root){}
        friend tmi_comparator;
    public:
        typedef const T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        typedef std::ptrdiff_t difference_type;
        using iterator_category = std::bidirectional_iterator_tag;
        using element_type = const T;
        iterator() = default;
        const T& operator*() const { return m_node->value(); }
        const T* operator->() const { return &m_node->value(); }
        iterator& operator++()
        {
            const node_type* next = tree_next(m_node);
            if (next) {
                m_node = next;
            } else {
                m_node = nullptr;
            }
            return *this;
        }
        iterator& operator--()
        {
            if (m_node) {
                const node_type* prev = tree_prev(m_node);
                if (prev) {
                    m_node = prev;
                } else {
                    m_node = nullptr;
                }
            } else {
                m_node = *m_root ? tree_max(*m_root) : nullptr;
            }
            return *this;
        }
        iterator operator++(int)
        {
            iterator copy(m_node, m_root);
            ++(*this);
            return copy;
        }
        iterator operator--(int)
        {
            iterator copy(m_node, m_root);
            --(*this);
            return copy;
        }
        bool operator==(iterator rhs) const { return m_node == rhs.m_node; }
        bool operator!=(iterator rhs) const { return m_node != rhs.m_node; }
    };
    using const_iterator = iterator;

    template <typename... Args>
    std::pair<iterator,bool> emplace(Args&&... args)
    {
        auto [node, success] = m_parent.do_emplace(std::forward<Args>(args)...);
        return std::make_pair(make_iterator(node), success);
    }

    std::pair<iterator,bool> insert(const T& value)
    {
        auto [node, success] = m_parent.do_insert(value);
        return std::make_pair(make_iterator(node), success);
    }

    iterator begin() const
    {
        if (m_root == nullptr)
            return end();
        return make_iterator(tree_min(m_root));
    }

    iterator end() const
    {
        return make_iterator(nullptr);
    }


    iterator iterator_to(const T& entry) const
    {
        T& ref = const_cast<T&>(entry);
        node_type* node = reinterpret_cast<node_type*>(&ref);
        return make_iterator(node);
    }

    template <typename Callable>
    bool modify(iterator it, Callable&& func)
    {
        node_type* node = const_cast<node_type*>(it.m_node);
        if (!node) return false;
        return m_parent.do_modify(node, std::forward<Callable>(func));
    }

    template<typename CompatibleKey>
    iterator find(const CompatibleKey& key) const
    {
        node_type* parent = nullptr;
        node_type* curr = m_root;
        while (curr != nullptr) {
            parent = curr;
            const auto& curr_key = m_key_from_value(curr->value());
            if (m_comparator(key, curr_key)) {
                curr = curr->template left<I>();
            } else if (m_comparator(curr_key, key)) {
                curr = curr->template right<I>();
            } else {
                return make_iterator(curr);
            }
        }
        return end();
    }

    template<typename CompatibleKey>
    iterator lower_bound(const CompatibleKey& key) const
    {
        node_type* curr = m_root;
        node_type* ret = nullptr;
        while (curr != nullptr) {
            const auto& curr_key = m_key_from_value(curr->value());
            if (!m_comparator(curr_key, key)) {
                ret = curr;
                curr = curr->template left<I>();
            } else {
                curr = curr->template right<I>();
            }
        }
        if (ret) {
            return make_iterator(ret);
        } else {
            return end();
        }
    }

    template<typename CompatibleKey>
    iterator upper_bound(const CompatibleKey& key) const
    {
        node_type* curr = m_root;
        node_type* ret = nullptr;
        while (curr != nullptr) {
            const auto& curr_key = m_key_from_value(curr->value());
            if (m_comparator(key, curr_key)) {
                ret = curr;
                curr = curr->template left<I>();
            } else {
                curr = curr->template right<I>();
            }
        }
        if (ret) {
            return make_iterator(ret);
        } else {
            return end();
        }
    }

    template<typename CompatibleKey>
    size_t count(const CompatibleKey& key) const
    {
        node_type* parent = nullptr;
        node_type* curr = m_root;
        size_t ret = 0;
        while (curr != nullptr) {
            parent = curr;
            const auto& curr_key = m_key_from_value(curr->value());
            if (m_comparator(key, curr_key)) {
                curr = curr->template left<I>();
            } else if (m_comparator(curr_key, key)) {
                curr = curr->template right<I>();
            } else {
                ret++;
                break;
            }
        }
        if constexpr (sorted_unique()) return ret;

        node_type* found_match = curr;
        if (found_match) {
            curr = tree_prev(found_match);
            while (curr != nullptr)
            {
                const auto& curr_key = m_key_from_value(curr->value());
                if (m_comparator(curr_key, key)) {
                    break;
                }
                ret++;
                curr = tree_prev(curr);
            }
            curr = tree_next(found_match);
            while (curr != nullptr)
            {
                const auto& curr_key = m_key_from_value(curr->value());
                if (m_comparator(key, curr_key)) {
                    break;
                }
                ret++;
                curr = tree_next(curr);
            }
        }
        return ret;
    }

    iterator erase(iterator it)
    {
        node_type* node = const_cast<node_type*>(it.m_node);
        node_type* next = tree_next(node);
        m_parent.do_erase(node);
        if (next) {
            return make_iterator(next);
        } else {
            return end();
        }
    }

    size_t erase(const key_type& key) const
    {
        node_type* parent = nullptr;
        node_type* curr = m_root;
        size_t ret = 0;
        while (curr != nullptr) {
            parent = curr;
            const auto& curr_key = m_key_from_value(curr->value());
            if (m_comparator(key, curr_key)) {
                curr = curr->template left<I>();
            } else if (m_comparator(curr_key, key)) {
                curr = curr->template right<I>();
            } else {
                ret++;
                break;
            }
        }
        node_type* found_match = curr;
        if (found_match) {
            if constexpr (!sorted_unique()) {
                curr = tree_prev(found_match);
                while (curr != nullptr)
                {
                    const auto& curr_key = m_key_from_value(curr->value());
                    if (m_comparator(curr_key, key)) {
                        break;
                    }
                    ret++;
                    node_type* to_erase = curr;
                    curr = tree_prev(curr);
                    m_parent.do_erase(to_erase);
                }
                curr = tree_next(found_match);
                while (curr != nullptr)
                {
                    const auto& curr_key = m_key_from_value(curr->value());
                    if (m_comparator(key, curr_key)) {
                        break;
                    }
                    ret++;
                    node_type* to_erase = curr;
                    curr = tree_next(curr);
                    m_parent.do_erase(to_erase);
                }
            }
            m_parent.do_erase(found_match);
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

    insert_return_type insert(node_handle&& handle)
    {
        node_type* node = handle.m_node;
        if(!node) {
            return {end(), false, {}};
        }
        node_type* conflict = m_parent.do_insert(node);
        if (conflict) {
            return {make_iterator(conflict), false, std::move(handle)};
        }
        handle.m_node = nullptr;
        return {make_iterator(node), true, {}};
    }

    node_handle extract(const_iterator it)
    {
        return m_parent.do_extract(const_cast<node_type*>(it.m_node));
    }

    allocator_type get_allocator() const noexcept
    {
        return m_parent.get_allocator();
    }

private:

    const node_type* node_from_iterator(iterator it) const
    {
        return it.m_node;
    }

    iterator make_iterator(const node_type* node) const
    {
        return iterator(node, &m_root);
    }

};

} // namespace tmi

#endif // TMI_COMPARATOR_H_
