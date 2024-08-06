// Copyright (c) 2024 Cory Fields
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TMI_H_
#define TMI_H_

#include "tminode.h"
#include "tmi_hasher.h"
#include "tmi_comparator.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <tuple>
#include <vector>

namespace tmi {

template <typename T, typename Comparators, typename Hashers, typename Allocator = std::allocator<T>>
class tmi
{
public:
    using allocator_type = Allocator;
    using comparator_types = Comparators::comparator_types;
    using hasher_types = Hashers::hasher_types;
    static constexpr int num_comparators = std::tuple_size_v<comparator_types>;
    static constexpr int num_hashers = std::tuple_size_v<hasher_types>;
    using node_type = tminode<T, num_comparators, num_hashers>;
    using node_allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<node_type>;
    using base_type = node_type::base_type;
    using comparator_base = tmi_comparator_base<T, num_comparators, num_hashers>;
    using hasher_base = tmi_hasher_base<T, num_comparators, num_hashers>;
    static_assert(num_comparators > 0 || num_hashers > 0, "No hashers or comparators defined");
    using hasher_insert_hints_type = hasher_insert_hints<T, num_comparators, num_hashers>;
    using comparator_insert_hints_type = comparator_insert_hints<T, num_comparators, num_hashers>;
    using hasher_premodify_cache_type = hasher_premodify_cache<T, num_comparators, num_hashers>;

    using comparator_hints_array = std::array<comparator_insert_hints_type, num_comparators>;

    using hasher_hints_array = std::array<hasher_insert_hints_type, num_hashers>;

    template <int I>
    using tmi_comparator_type = tmi_comparator<T, num_comparators, num_hashers, I, std::tuple_element_t<I, comparator_types>>;

    template <int I>
    using tmi_hasher_type = tmi_hasher<T, num_comparators, num_hashers, I, std::tuple_element_t<I, hasher_types>>;

    template <int I>
    using sort_iterator = tmi_comparator_type<I>::iterator;

private:
    node_type* m_begin{nullptr};
    node_type* m_end{nullptr};
    size_t m_size{0};

    std::array<comparator_base*, num_comparators> m_comparator_instances;
    std::array<hasher_base*, num_hashers> m_hasher_instances;

    node_allocator_type m_alloc;


    template <int I = 0, class Callable, typename Node, typename... Args>
    static void foreach_hasher(Callable&& func, Node node, Args&&... args)
    {
        if constexpr (num_hashers) {
            func.template operator()<I>(node, std::get<I>(args)...);
        }
        if constexpr (I + 1 < num_hashers) {
            foreach_hasher<I + 1>(std::forward<Callable>(func), node, std::forward<Args>(args)...);
        }
    }

    template <int I = 0, class Callable, typename Node, typename... Args>
    static bool get_foreach_hasher(Callable&& func, Node node, Args&&... args)
    {
        if constexpr (num_hashers) {
            if (!func.template operator()<I>(node, std::get<I>(args)...)) {
                return false;
            }
        }
        if constexpr (I + 1 < num_hashers) {
            return get_foreach_hasher<I + 1>(std::forward<Callable>(func), node, std::forward<Args>(args)...);
        }
        return true;
    }

    template <int I = 0, class Callable, typename Node, typename... Args>
    static void foreach_comparator(Callable&& func, Node node, Args&&... args)
    {
        if constexpr (num_comparators) {
            func.template operator()<I>(node, std::get<I>(args)...);
        }
        if constexpr (I + 1 < num_comparators) {
            foreach_comparator<I + 1>(std::forward<Callable>(func), node, std::forward<Args>(args)...);
        }
    }

    template <int I = 0, class Callable, typename Node, typename... Args>
    static bool get_foreach_comparator(Callable&& func, Node node, Args&&... args)
    {
        if constexpr (num_comparators) {
            if (!func.template operator()<I>(node, std::get<I>(args)...)) {
                return false;
            }
        }
        if constexpr (I + 1 < num_comparators) {
            return get_foreach_comparator<I + 1>(std::forward<Callable>(func), node, std::forward<Args>(args)...);
        }
        return true;
    }

    template <int I>
    const auto& get_hasher_instance() const
    {
        return *static_cast<const tmi_hasher_type<I>*>(std::get<I>(m_hasher_instances));
    }

    template <int I>
    auto& get_hasher_instance()
    {
        return *static_cast<tmi_hasher_type<I>*>(std::get<I>(m_hasher_instances));
    }

    template <int I>
    const auto& get_comparator_instance() const
    {
        return *static_cast<const tmi_comparator_type<I>*>(std::get<I>(m_comparator_instances));
    }

    template <int I>
    auto& get_comparator_instance()
    {
        return *static_cast<tmi_comparator_type<I>*>(std::get<I>(m_comparator_instances));
    }


    template <int I, typename H>
    node_type* do_find(const H::hash_type& hash_key) const
    {
        if constexpr (std::is_same_v<H, typename std::tuple_element_t<I, hasher_types>>) {
            return get_hasher_instance<I>().find_hash(hash_key);
        } else if constexpr (I + 1 < num_hashers) {
            return do_find<I + 1, H>(hash_key);
        } else {
            // g++ isn't able to cope with this static_assert for some reason.
            // This should be unreachable code.
            //static_assert(false, "Invalid hasher");
            return nullptr;
        }
    }

    bool do_insert(node_type* node)
    {
        comparator_hints_array comp_hints;
        hasher_hints_array hash_hints;
        bool can_insert;

        can_insert = get_foreach_hasher([this]<int I>(node_type* node, auto& hints) {
            if (!get_hasher_instance<I>().preinsert_node_hash(node, hints)) return false;
            return true;
        }, node, hash_hints);

        if (!can_insert) return false;

        can_insert = get_foreach_comparator([this]<int I>(node_type* node, auto& hints) {
            if (!get_comparator_instance<I>().preinsert_node_comparator(node, hints)) return false;
            return true;
        }, node, comp_hints);

        if (!can_insert) return false;

        foreach_hasher([this]<int I>(node_type* node, auto& hints) {
            get_hasher_instance<I>().insert_node_hash(node, hints);
        }, node, hash_hints);

        foreach_comparator([this]<int I>(node_type* node, auto& hints) {
            get_comparator_instance<I>().insert_node_comparator(node, hints);
        }, node, comp_hints);

        if (m_begin == nullptr) {
            assert(m_end == nullptr);
            m_begin = m_end = node;
        } else {
            m_end = node;
        }

        m_size++;
        return true;
    }

    void do_erase_cleanup(node_type* node)
    {
        if (node == m_end) {
            m_end = node->prev();
        }
        if (node == m_begin) {
            m_begin = node->next();
        }
        node->unlink();
        std::allocator_traits<node_allocator_type>::destroy(m_alloc, node);
        std::allocator_traits<node_allocator_type>::deallocate(m_alloc, node, 1);
        m_size--;
    }

    void do_erase(node_type* node)
    {
        foreach_comparator([this]<int I>(node_type* node) {
            get_comparator_instance<I>().tree_remove(node->get_base());
        }, node);

        foreach_hasher([this]<int I>(node_type* node) {
            get_hasher_instance<I>().hash_remove_direct(node->get_base());
        }, node);
        do_erase_cleanup(node);
    }

    template <typename Callable>
    bool do_modify(node_type* node, Callable&& func)
    {
        struct hash_modify_actions {
            bool m_do_reinsert{false};
        };
        struct comparator_modify_actions {
            bool m_do_reinsert{false};
        };

        using hasher_premodify_cache_array = std::array<hasher_premodify_cache_type, num_hashers>;
        using hasher_modify_actions_array = std::array<hash_modify_actions, num_hashers>;
        using comparator_modify_actions_array = std::array<comparator_modify_actions, num_comparators>;


        // Create a cache of the pre-modified hash buckets
        hasher_premodify_cache_array hash_cache;
        foreach_hasher([this]<int I>(node_type* node, auto& cache) {
            get_hasher_instance<I>().hasher_create_premodify_cache(node, cache);
        }, node, hash_cache);


        func(node->value());

        hasher_modify_actions_array hash_modify;
        comparator_modify_actions_array comp_modify;

        // Erase modified hashes
        foreach_hasher([this]<int I>(node_type* node, auto& modify, const auto& cache) {
            modify.m_do_reinsert = get_hasher_instance<I>().hasher_erase_if_modified(node, cache);
         }, node, hash_modify, hash_cache);


        // Erase modified sorts
        foreach_comparator([this]<int I>(node_type* node, auto& modify) {
            modify.m_do_reinsert = get_comparator_instance<I>().comparator_erase_if_modified(node);
        }, node, comp_modify);

        // At this point the node has been removed from all buckets and trees.
        // Test to see if it's reinsertable everywhere or delete it.

        comparator_hints_array comp_hints;
        hasher_hints_array hash_hints;

        // Check to see if any new hashes can be safely inserted
        bool insertable = get_foreach_hasher([this]<int I>(node_type* node, const auto& modify, auto& hints) {
            if (modify.m_do_reinsert) return get_hasher_instance<I>().preinsert_node_hash(node, hints);
            return true;
        }, node, hash_modify, hash_hints);

        // Check to see if any new sorts can be safely inserted
        if (insertable) {
            insertable = get_foreach_comparator([this]<int I>(node_type* node, const auto& modify, auto& hints) {
                if (modify.m_do_reinsert) return get_comparator_instance<I>().preinsert_node_comparator(node, hints);
                return true;
            }, node, comp_modify, comp_hints);
        }

        if (!insertable) {
            do_erase_cleanup(node);
            return false;
        }

        foreach_hasher([this]<int I>(node_type* node, const auto& modify, const auto& hints) {
            if (modify.m_do_reinsert) get_hasher_instance<I>().insert_node_hash(node, hints);
            return true;
        },
                       node, hash_modify, hash_hints);

        foreach_comparator([this]<int I>(node_type* node, const auto& modify, const auto& hints) {
            if (modify.m_do_reinsert) get_comparator_instance<I>().insert_node_comparator(node, hints);
            return true;
        },
                           node, comp_modify, comp_hints);

        return true;
    }

    template <int I = 0, typename Index>
    auto do_project(node_type* node) const
    {
        if constexpr (I < num_comparators && std::is_same_v<Index, typename std::tuple_element_t<I, comparator_types>>) {
            return sort_iterator<I>(node);
        }
        if constexpr (I < num_hashers && std::is_same_v<Index, typename std::tuple_element_t<I, hasher_types>>) {
            return hash_iterator<I>(node);
        }
        if constexpr (I + 1 < num_comparators || I + 1 < num_hashers) {
            return do_project<I + 1, Index>(node);
        }
    }

    template <int I = 0, typename Index>
    auto do_get() const
    {
        if constexpr (I < num_comparators && std::is_same_v<Index, typename std::tuple_element_t<I, comparator_types>>) {
            return get_comparator_instance<I>();
        }
        if constexpr (I < num_hashers && std::is_same_v<Index, typename std::tuple_element_t<I, hasher_types>>) {
            return get_hasher_instance<I>();
        }
        if constexpr (I + 1 < num_comparators || I + 1 < num_hashers) {
            return do_get<I + 1, Index>();
        }
    }

public:

    class iterator
    {
        node_type* m_node{};

    public:
        friend class tmi;
        friend class const_iterator;
        typedef T value_type;
        typedef T* pointer;
        typedef T& reference;
        using element_type = T;
        iterator() = default;
        template <int I>
        iterator(sort_iterator<I> it) : m_node(it.m_node)
        {
        }
        iterator(node_type* node) : m_node(node) {}
        T& operator*() const { return m_node->value(); }
        T* operator->() const { return &m_node->value(); }
        iterator& operator++()
        {
            m_node = m_node->next();
            return *this;
        }
        iterator& operator--()
        {
            m_node = m_node->prev();
            return *this;
        }
        iterator operator++(int)
        {
            iterator copy(m_node);
            ++(*this);
            return copy;
        }
        iterator operator--(int)
        {
            iterator copy(m_node);
            --(*this);
            return copy;
        }
        bool operator==(iterator rhs) const { return m_node == rhs.m_node; }
        bool operator!=(iterator rhs) const { return m_node != rhs.m_node; }
    };

    class const_iterator
    {
        const node_type* m_node{};

    public:
        friend class tmi;
        typedef const T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        using element_type = const T;
        const_iterator() = default;
        const_iterator(iterator it) : m_node(it.m_node) {}
        const_iterator(const node_type* node) : m_node(node) {}
        const T& operator*() const { return m_node->value(); }
        const T* operator->() const { return &m_node->value(); }
        const_iterator& operator++()
        {
            m_node = m_node->next();
            return *this;
        }
        const_iterator& operator--()
        {
            m_node = m_node->prev();
            return *this;
        }
        const_iterator operator++(int)
        {
            const_iterator copy(m_node);
            ++(*this);
            return copy;
        }
        const_iterator operator--(int)
        {
            const_iterator copy(m_node);
            --(*this);
            return copy;
        }
        bool operator==(const_iterator rhs) const { return m_node == rhs.m_node; }
        bool operator!=(const_iterator rhs) const { return m_node != rhs.m_node; }
    };

    tmi(const allocator_type& alloc = {}) :  m_alloc(alloc)
    {
        foreach_hasher([this]<int I>(std::nullptr_t, hasher_base*& hasher) {
            using hasher_instance_type = tmi_hasher_type<I>;
            hasher = new hasher_instance_type();
         }, nullptr, m_hasher_instances);


        foreach_comparator([this]<int I>(std::nullptr_t, comparator_base*& hasher) {
            using comparator_instance_type = tmi_comparator_type<I>;
            hasher = new comparator_instance_type();
         }, nullptr, m_comparator_instances);
    }

    ~tmi()
    {
        clear();

        foreach_hasher([this]<int I>(std::nullptr_t, hasher_base*& hasher) {
            using hasher_instance_type = tmi_hasher_type<I>;
            hasher_instance_type* to_delete = static_cast<hasher_instance_type*>(hasher);
            delete to_delete;
         }, nullptr, m_hasher_instances);

        foreach_comparator([this]<int I>(std::nullptr_t, comparator_base*& comparator) {
            using comparator_instance_type = tmi_comparator_type<I>;
            comparator_instance_type* to_delete = static_cast<comparator_instance_type*>(comparator);
            delete to_delete;
         }, nullptr, m_comparator_instances);

    }

    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    {
        node_type* node = m_alloc.allocate(1);
        node = std::uninitialized_construct_using_allocator<node_type>(node, m_alloc, m_end, std::forward<Args>(args)...);
        bool inserted = do_insert(node);
        if (!inserted) {
            node = nullptr;
            std::allocator_traits<node_allocator_type>::destroy(m_alloc, node);
            std::allocator_traits<node_allocator_type>::deallocate(m_alloc, node, 1);
        }
        return std::make_pair(iterator(node), inserted);
    }

    void clear()
    {
        auto* node = m_begin;
        while (node) {
            auto* to_delete = node;
            node = node->next();
            std::allocator_traits<node_allocator_type>::destroy(m_alloc, to_delete);
            std::allocator_traits<node_allocator_type>::deallocate(m_alloc, to_delete, 1);
        }
        m_begin = m_end = nullptr;

        foreach_hasher([this]<int I>(std::nullptr_t) {
            get_hasher_instance<I>().clear();
         }, nullptr);

        foreach_hasher([this]<int I>(std::nullptr_t) {
            get_comparator_instance<I>().clear();
         }, nullptr);

    }

    iterator erase(const_iterator it)
    {
        node_type* node = const_cast<node_type*>(it.m_node);
        node_type* ret = node->next();
        do_erase(node);
        return iterator(ret);
    }


    template <typename Callable>
    bool modify(const_iterator it, Callable&& func)
    {
        node_type* node = const_cast<node_type*>(it.m_node);
        if (!node) {
            return false;
        }
        return do_modify(node, std::forward<Callable>(func));
    }

    size_t size() const { return m_size; }

    iterator begin() const { return iterator(m_begin); }

    iterator end() const { return iterator(nullptr); }

    template <typename H>
    iterator find(const H::hash_type& hashable) const
    {
        return iterator(do_find<0, H>(hashable));
    }

    template <int I>
    sort_iterator<I> sort_begin() const
    {
        return get_comparator_instance<I>().begin();
    }

    template <int I>
    sort_iterator<I> sort_end() const
    {
        return get_comparator_instance<I>().end();
    }

    iterator iterator_to(const T& entry) const
    {
        T& ref = const_cast<T&>(entry);
        node_type* node = reinterpret_cast<node_type*>(&ref);
        return iterator(node);
    }
    static constexpr size_t node_size()
    {
        return sizeof(node_type);
    }

    template <typename Index>
    auto project(auto it) const
    {
        return do_project<0, Index>(it.m_node);
    }

    template <typename Index>
    auto get() const
    {
        return do_get<0, Index>();
    }

};

} // namespace tmi

#endif // TMI_H_
