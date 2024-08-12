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

template <typename Tag, typename Indices, size_t I = 0>
static constexpr size_t get_index_for_tag()
{
    using index_types = typename Indices::index_types;
    constexpr size_t num_indices = std::tuple_size<index_types>();
    if constexpr (std::is_same_v<typename std::tuple_element_t<I, index_types>::tag, Tag>) return I;
    else if constexpr (I + 1 < num_indices) return get_index_for_tag<Tag, Indices, I + 1>();
    else return num_indices;
}

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
    static constexpr size_t num_indices = std::tuple_size<index_types>();

    template <int I>
    struct nth_index
    {
        using value = typename std::tuple_element_t<I, index_types>::sorted;
        using comparator = tmi_comparator<T, node_type, std::tuple_element_t<I, index_types>, parent_type, I>;
        using hasher = tmi_hasher<T, node_type, std::tuple_element_t<I, index_types>, parent_type, I>;
    public:
        using type = std::conditional_t<std::is_same_v<value, std::true_type>, comparator, hasher>;
    };

    template <typename Tag>
    struct index
    {
        static constexpr size_t value = detail::get_index_for_tag<Tag, Indices>();
        static_assert(value < num_indices, "tag not found");
        using type = typename nth_index<value>::type;
    };

    template <int I>
    using maybe_null_tmi_index_type = typename detail::maybe_null_index_type_helper<T, Indices, Allocator, tmi<T, Indices, Allocator>, I>::type; 

    template <int I>
    using tmi_index_type = typename nth_index<I>::type;

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
    const auto& get_index_instance() const noexcept
    {
        if constexpr(I == 0) {
            return(*static_cast<const inherited_index*>(this));
        } else {
            return std::get<I>(m_index_instances);
        }
    }

    template <int I>
    auto& get_index_instance() noexcept
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

        indices_premodify_cache_tuple index_cache;

        foreach_index([this]<int I>(node_type* node, auto& cache) {
            if constexpr (tmi_index_type<I>::requires_premodify_cache()) {
                get_index_instance<I>().create_premodify_cache(node, cache);
            }
        }, node, index_cache);


        func(node->value());

        std::array<bool, num_indices> indicies_to_modify{};

        foreach_index([this]<int I>(node_type* node, auto& modify, const auto& cache) {
            modify = get_index_instance<I>().erase_if_modified(node, cache);
         }, node, indicies_to_modify, index_cache);


        indices_hints_tuple index_hints;

        bool insertable = get_foreach_index([this]<int I>(node_type* node, const auto& modify, auto& hints) {
            if (modify) return get_index_instance<I>().preinsert_node(node, hints) == nullptr;
            return true;
        }, node, indicies_to_modify, index_hints);

        if (!insertable) {
            do_erase_cleanup(node);
            return false;
        }

        foreach_index([this]<int I>(node_type* node, const auto& modify, const auto& hints) {
            if (modify) get_index_instance<I>().insert_node(node, hints);
        }, node, indicies_to_modify, index_hints);

        return true;
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


    template<size_t I, typename IteratorType>
    typename nth_index<I>::type::iterator project(IteratorType it)
    {
        return get_index_instance<I>().make_iterator(it.m_node);
    }

    template<size_t I, typename IteratorType>
    typename nth_index<I>::type::const_iterator project(IteratorType it) const
    {
        return get_index_instance<I>().make_iterator(it.m_node);
    }

    template<typename Tag, typename IteratorType>
    typename index<Tag>::type::iterator project(IteratorType it)
    {
        return get_index_instance<index<Tag>::value>().make_iterator(it.m_node);
    }

    template<typename Tag,typename IteratorType>
    typename index<Tag>::type::const_iterator project(IteratorType it) const
    {
        return get_index_instance<index<Tag>::value>().make_iterator(it.m_node);
    }

    template<size_t I>
    typename nth_index<I>::type& get() noexcept
    {
        return get_index_instance<I>();
    }

    template<int I> const typename
    nth_index<I>::type& get() const noexcept
    {
        return get_index_instance<I>();
    }

    template<typename Tag>
    typename index<Tag>::type& get() noexcept
    {
        return get_index_instance<index<Tag>::value>();
    }

    template<typename Tag>
    const typename index<Tag>::type& get() const noexcept
    {
        return get_index_instance<index<Tag>::value>();
    }
};

} // namespace tmi

#endif // TMI_H_
