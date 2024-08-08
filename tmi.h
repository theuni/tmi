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

namespace detail {

/*
    Helper to find the first index which tmi should inherit from
*/
template <typename T, typename Indices, typename Allocator, typename Parent>
struct first_index_type_helper
{
    using index_types = typename Indices::index_types;
    using node_type = tminode<T, Indices>;
    using value = typename std::tuple_element_t<0, index_types>::sorted;
    using comparator = tmi_comparator<T, node_type, std::tuple_element_t<0, index_types>, Parent, 0>;
    using hasher = tmi_hasher<T, node_type, std::tuple_element_t<0, index_types>, Parent, 0>;
    using type = std::conditional_t<std::is_same_v<value, std::true_type>, comparator, hasher>;
};

/* These null helpers allow for a dummy instance to be created as the first
   tuple member. Later, get_instance will refer back to *this rather than
   the dummy.
*/

template <typename T, typename Indices, typename Allocator, typename Parent, int I>
struct maybe_null_index_type_helper
{
    using index_types = typename Indices::index_types;
    using node_type = tminode<T, Indices>;
    using value = typename std::tuple_element_t<I, index_types>::sorted;
    using comparator = tmi_comparator<T, node_type, std::tuple_element_t<I, index_types>, Parent, I>;
    using hasher = tmi_hasher<T, node_type, std::tuple_element_t<I, index_types>, Parent, I>;
    using type = std::conditional_t<std::is_same_v<value, std::true_type>, comparator, hasher>;
    struct null_instance{
        null_instance(Parent& parent) : m_parent(parent){}
        Parent& m_parent;
    };
};

template <typename T, typename Indices, typename Allocator, typename Parent>
struct maybe_null_index_type_helper<T, Indices, Allocator, Parent, 0>
{
    struct null_instance{
        null_instance(Parent& parent) : m_parent(parent){}
        Parent& m_parent;
    };
    using type = null_instance;
};

}

template <typename T, typename Indices, typename Allocator = std::allocator<T>>
class tmi : public detail::first_index_type_helper<T, Indices, Allocator, tmi<T, Indices, Allocator>>::type
{
public:
    using parent_type = tmi<T, Indices, Allocator>;
    using allocator_type = Allocator;
    using index_types = typename Indices::index_types;
    using node_type = tminode<T, Indices>;
    using node_allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<node_type>;
    using base_type = typename node_type::base_type;
    using inherited_index = typename detail::first_index_type_helper<T, Indices, Allocator, tmi<T, Indices, Allocator>>::type;

    template <int I>
    struct index_type_helper
    {
        using value = typename std::tuple_element_t<I, index_types>::sorted;
        using comparator = tmi_comparator<T, node_type, std::tuple_element_t<I, index_types>, parent_type, I>;
        using hasher = tmi_hasher<T, node_type, std::tuple_element_t<I, index_types>, parent_type, I>;
        using type = std::conditional_t<std::is_same_v<value, std::true_type>, comparator, hasher>;
    };

    template <int I>
    using maybe_null_tmi_index_type = typename detail::maybe_null_index_type_helper<T, Indices, Allocator, tmi<T, Indices, Allocator>, I>::type; 

    template <int I>
    using tmi_index_type = typename index_type_helper<I>::type;

    template <typename>
    struct index_helper;
    template <size_t... ints>
    struct index_helper<std::index_sequence<ints...>> {
        using index_types = std::tuple<maybe_null_tmi_index_type<ints> ...>;
        using hints_types =  std::tuple<typename tmi_index_type<ints>::insert_hints ...>;
        using premodify_cache_types = std::tuple<typename tmi_index_type<ints>::premodify_cache ...>;

        static index_types make_index_types(parent_type& parent) {
            return std::make_tuple( maybe_null_tmi_index_type<ints>(parent) ...);
        }
    };

    static constexpr size_t num_indices = std::tuple_size<index_types>();
    using indices_tuple = typename index_helper<std::make_index_sequence<num_indices>>::index_types;
    using indices_hints_tuple = typename index_helper<std::make_index_sequence<num_indices>>::hints_types;
    using indices_premodify_cache_tuple = typename index_helper<std::make_index_sequence<num_indices>>::premodify_cache_types;


    template <typename, typename, typename, typename, int>
    friend class tmi_hasher;

    template <typename, typename, typename, typename, int>
    friend class tmi_comparator;

private:
    node_type* m_begin{nullptr};
    node_type* m_end{nullptr};
    size_t m_size{0};


    indices_tuple m_index_instances;

    node_allocator_type m_alloc;


    template <int I = 0, class Callable, typename Node, typename... Args>
    static void foreach_index(Callable&& func, Node node, Args&&... args)
    {
        if constexpr (num_indices) {
            func.template operator()<I>(node, std::get<I>(args)...);
        }
        if constexpr (I + 1 < num_indices) {
            foreach_index<I + 1>(std::forward<Callable>(func), node, std::forward<Args>(args)...);
        }
    }

    template <int I = 0, class Callable, typename Node, typename... Args>
    static bool get_foreach_index(Callable&& func, Node node, Args&&... args)
    {
        if constexpr (num_indices) {
            if (!func.template operator()<I>(node, std::get<I>(args)...)) {
                return false;
            }
        }
        if constexpr (I + 1 < num_indices) {
            return get_foreach_index<I + 1>(std::forward<Callable>(func), node, std::forward<Args>(args)...);
        }
        return true;
    }

    template <int I>
    const auto& get_index_instance() const
    {
        if constexpr(I == 0) {
            return(*static_cast<const inherited_index*>(this));
        } else {
            return std::get<I>(m_index_instances);
        }
    }

    template <int I>
    auto& get_index_instance()
    {
        if constexpr(I == 0) {
            return(*static_cast<inherited_index*>(this));
        } else {
            return std::get<I>(m_index_instances);
        }
    }

    node_type* do_insert(node_type* node)
    {
        indices_hints_tuple hints;

        bool can_insert;
        node_type* conflict = nullptr;
        can_insert = get_foreach_index([this, &conflict]<int I>(node_type* node, auto& hints) {
            conflict = get_index_instance<I>().preinsert_node(node, hints);
            return conflict == nullptr;
        }, node, hints);

        if (!can_insert) return conflict;

        foreach_index([this]<int I>(node_type* node, auto& hints) {
            get_index_instance<I>().insert_node(node, hints);
        }, node, hints);

        if (m_begin == nullptr) {
            assert(m_end == nullptr);
            m_begin = m_end = node;
        } else {
            m_end = node;
        }

        m_size++;
        return nullptr;
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

    template <typename... Args>
    std::pair<node_type*, bool> do_emplace(Args&&... args)
    {
        node_type* node = m_alloc.allocate(1);
        node = std::uninitialized_construct_using_allocator<node_type>(node, m_alloc, m_end, std::forward<Args>(args)...);
        node_type* conflict = do_insert(node);
        if (conflict != nullptr) {
            std::allocator_traits<node_allocator_type>::destroy(m_alloc, node);
            std::allocator_traits<node_allocator_type>::deallocate(m_alloc, node, 1);
            return std::make_pair(conflict, false);
        }
        return std::make_pair(node, true);
    }

    void do_erase(node_type* node)
    {
        foreach_index([this]<int I>(node_type* node) {
            get_index_instance<I>().remove_node(node);
        }, node);
        do_erase_cleanup(node);
    }

    template <typename Callable>
    bool do_modify(node_type* node, Callable&& func)
    {
        struct modify_actions {
            bool m_do_reinsert{false};
        };
        using index_modify_actions_array = std::array<modify_actions, num_indices>;

        indices_premodify_cache_tuple index_cache;

        foreach_index([this]<int I>(node_type* node, auto& cache) {
            if constexpr (tmi_index_type<I>::requires_premodify_cache()) {
                get_index_instance<I>().create_premodify_cache(node, cache);
            }
        }, node, index_cache);


        func(node->value());

        index_modify_actions_array index_modify;

        foreach_index([this]<int I>(node_type* node, auto& modify, const auto& cache) {
            modify.m_do_reinsert = get_index_instance<I>().erase_if_modified(node, cache);
         }, node, index_modify, index_cache);


        indices_hints_tuple index_hints;

        bool insertable = get_foreach_index([this]<int I>(node_type* node, const auto& modify, auto& hints) {
            if (modify.m_do_reinsert) return get_index_instance<I>().preinsert_node(node, hints) == nullptr;
            return true;
        }, node, index_modify, index_hints);

        if (!insertable) {
            do_erase_cleanup(node);
            return false;
        }

        foreach_index([this]<int I>(node_type* node, const auto& modify, const auto& hints) {
            if (modify.m_do_reinsert) get_index_instance<I>().insert_node(node, hints);
            return true;
        }, node, index_modify, index_hints);

        return true;
    }

    template <int I = 0, typename Index>
    const auto& do_get() const
    {
        if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, index_types>>) {
            return get_index_instance<I>();
        } else if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, index_types>::tag>) {
            return get_index_instance<I>();
        } else if constexpr (I + 1 < num_indices) {
            return do_get<I + 1, Index>();
        }
    }

    template <int I = 0, typename Index>
    auto& do_get()
    {
        if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, index_types>>) {
            return get_index_instance<I>();
        } else if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, index_types>::tag>) {
            return get_index_instance<I>();
        } else if constexpr (I + 1 < num_indices) {
            return do_get<I + 1, Index>();
        }
    }

    void do_clear()
    {
        foreach_index([this]<int I>(std::nullptr_t) {
            get_index_instance<I>().do_clear();
         }, nullptr);

        auto* node = m_begin;
        while (node) {
            auto* to_delete = node;
            node = node->next();
            std::allocator_traits<node_allocator_type>::destroy(m_alloc, to_delete);
            std::allocator_traits<node_allocator_type>::deallocate(m_alloc, to_delete, 1);
        }
        m_begin = m_end = nullptr;
    }

public:

    tmi(const allocator_type& alloc = {})
        : inherited_index(*this),
          m_index_instances(index_helper<std::make_index_sequence<num_indices>>::make_index_types(*this)),
          m_alloc(alloc)
    {
    }

    ~tmi()
    {
        do_clear();
    }

    static constexpr size_t node_size()
    {
        return sizeof(node_type);
    }

    template <typename Index>
    auto project(auto it) const
    {
        return do_get<0, Index>().make_iterator(it.m_node);
    }

    template <typename Index>
    auto& get()
    {
        return do_get<0, Index>();
    }

    template <typename Index>
    const auto& get() const
    {
        return do_get<0, Index>();
    }

};

} // namespace tmi

#endif // TMI_H_
