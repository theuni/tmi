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

struct hashed_type{};
struct ordered_type{};
struct tag_type{};

struct tag_dummy : tag_type
{
    struct empty{};
    using type = std::tuple<empty>;
};

template < typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct hashed_args
{
    static constexpr bool using_tags = std::is_base_of_v<tag_type, Arg1>;
    using tags_arg = std::conditional_t<using_tags, Arg1, tag_dummy>;
    using key_from_value_type = std::conditional_t<using_tags, Arg2, Arg1>;
    static_assert(!std::is_same_v<key_from_value_type, void>);
    using hasher_arg = std::conditional_t<using_tags, Arg3, Arg2>;
    using pred_arg = std::conditional_t<using_tags, Arg4, Arg3>;

    using default_hasher = std::hash<typename key_from_value_type::result_type>;
    using default_pred = std::equal_to<typename key_from_value_type::result_type>;

    using hasher_type = std::conditional_t<std::is_same_v<hasher_arg, void>, default_hasher, hasher_arg>;
    using pred_type = std::conditional_t<std::is_same_v<pred_arg, void>, default_pred, pred_arg>;
    using tags = typename tags_arg::type;
};

template<typename Arg1, typename Arg2, typename Arg3>
struct ordered_args
{
    static constexpr bool using_tags = std::is_base_of_v<tag_type, Arg1>;
    using tags_arg = std::conditional_t<using_tags, Arg1, tag_dummy>;
    using key_from_value_type = std::conditional_t<using_tags, Arg2, Arg1>;
    static_assert(!std::is_same_v<key_from_value_type, void>);
    using comparator_arg = std::conditional_t<using_tags, Arg3, Arg2>;

    using default_comparator = std::less<typename key_from_value_type::result_type>;

    using comparator = std::conditional_t<std::is_same_v<comparator_arg, void>, default_comparator, comparator_arg>;
    using tags = typename tags_arg::type;
};

template <typename T, typename Indices, typename Allocator, typename Parent, int I>
struct index_type_helper
{
    using index_types = typename Indices::index_types;
    using node_type = tminode<T, Indices>;
    using index_type = std::tuple_element_t<I, index_types>;
    using comparator = tmi_comparator<T, node_type, index_type, Parent, I>;
    using hasher = tmi_hasher<T, node_type, index_type, Parent, I>;
    using type = std::conditional_t<std::is_base_of_v<hashed_type, index_type>, hasher, comparator>;
};

} // namespace detail

template<typename... Tags>
struct tag : detail::tag_type
{
   using type = std::tuple<Tags...>;
};

template <typename Value>
struct identity
{
    using result_type = Value;
    const Value& operator()(const Value& val) { return val; }
};

template < typename Arg1, typename Arg2=void, typename Arg3=void, typename Arg4=void>
struct hashed_unique : detail::hashed_type, public detail::hashed_args<Arg1, Arg2, Arg3, Arg4>
{
    static constexpr bool is_hashed_unique() { return true; }
};

template < typename Arg1, typename Arg2=void, typename Arg3=void, typename Arg4=void>
struct hashed_non_unique : detail::hashed_type, public detail::hashed_args<Arg1, Arg2, Arg3, Arg4>
{
    static constexpr bool is_hashed_unique() { return false; }
};

template<typename Arg1, typename Arg2 = void, typename Arg3 = void>
struct ordered_unique : detail::ordered_type, public detail::ordered_args<Arg1, Arg2, Arg3>
{
    static constexpr bool is_ordered_unique() { return true; }
};

template<typename Arg1, typename Arg2 = void, typename Arg3 = void>
struct ordered_non_unique : detail::ordered_type, public detail::ordered_args<Arg1, Arg2, Arg3>
{
    static constexpr bool is_ordered_unique() { return false; }
};

template<typename... Indices>
struct indexed_by
{
   using index_types = std::tuple<Indices...>;
};

template <typename T, typename Indices, typename Allocator = std::allocator<T>>
class tmi : public detail::index_type_helper<T, Indices, Allocator, tmi<T, Indices, Allocator>, 0>::type
{
public:
    using parent_type = tmi<T, Indices, Allocator>;
    using allocator_type = Allocator;
    using index_types = typename Indices::index_types;
    using node_type = tminode<T, Indices>;
    using node_allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<node_type>;
    using inherited_index = typename detail::index_type_helper<T, Indices, Allocator, tmi<T, Indices, Allocator>, 0>::type;
    static constexpr size_t num_indices = std::tuple_size<index_types>();

    template <int I>
    struct nth_index
    {
        using type = typename detail::index_type_helper<T, Indices, Allocator, tmi<T, Indices, Allocator>, I>::type;
    };

    template <typename Tag>
    struct index
    {
        template <size_t I = 0, size_t J = 0>
        static constexpr size_t get_index_for_tag()
        {
            using tags = typename std::tuple_element_t<I, index_types>::tags;
            constexpr size_t tag_count = std::tuple_size<tags>();
            using tag = typename std::tuple_element_t<J, tags>;

            if constexpr (std::is_same_v<tag, Tag>) return I;
            else if constexpr (J + 1 < tag_count) return get_index_for_tag<I, J + 1>();
            else if constexpr (I + 1 < num_indices) return get_index_for_tag<I + 1, 0>();
            else return num_indices;
        }
        static constexpr size_t value = get_index_for_tag();
        static_assert(value < num_indices, "tag not found");
        using type = typename nth_index<value>::type;
    };

    template <typename>
    struct index_tuple_helper;
    template <size_t First, size_t... ints>
    struct index_tuple_helper<std::index_sequence<First, ints...>> {
        using index_types = std::tuple<inherited_index&, typename nth_index<ints>::type ...>;
        using hints_types =  std::tuple<typename nth_index<First>::type::insert_hints, typename nth_index<ints>::type::insert_hints ...>;
        using premodify_cache_types = std::tuple<typename nth_index<First>::type::premodify_cache, typename nth_index<ints>::type::premodify_cache ...>;

        static index_types make_index_types(parent_type& parent) {
            return std::make_tuple(std::ref(parent), typename nth_index<ints>::type(parent) ...);
        }
    };

    using indices_tuple = typename index_tuple_helper<std::make_index_sequence<num_indices>>::index_types;
    using indices_hints_tuple = typename index_tuple_helper<std::make_index_sequence<num_indices>>::hints_types;
    using indices_premodify_cache_tuple = typename index_tuple_helper<std::make_index_sequence<num_indices>>::premodify_cache_types;


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
    const typename nth_index<I>::type& get_index_instance() const noexcept
    {
        return std::get<I>(m_index_instances);
    }

    template <int I>
    typename nth_index<I>::type& get_index_instance() noexcept
    {
        return std::get<I>(m_index_instances);
    }

    node_type* do_insert(node_type* node)
    {
        indices_hints_tuple hints;

        bool can_insert;
        node_type* conflict = nullptr;
        can_insert = get_foreach_index([&conflict]<int I>(node_type* node, typename nth_index<I>::type& instance, auto& hints) {
            conflict = instance.preinsert_node(node, hints);
            return conflict == nullptr;
        }, node, m_index_instances, hints);

        if (!can_insert) return conflict;

        foreach_index([]<int I>(node_type* node, typename nth_index<I>::type& instance, const auto& hints) {
            instance.insert_node(node, hints);
        }, node, m_index_instances,  hints);

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
        foreach_index([]<int I>(node_type* node, typename nth_index<I>::type& instance) {
            instance.remove_node(node);
        }, node, m_index_instances);
        do_erase_cleanup(node);
    }

    template <typename Callable>
    bool do_modify(node_type* node, Callable&& func)
    {
        indices_premodify_cache_tuple index_cache;

        foreach_index([]<int I>(node_type* node, typename nth_index<I>::type& instance, auto& cache) {
            if constexpr (nth_index<I>::type::requires_premodify_cache()) {
                instance.create_premodify_cache(node, cache);
            }
        }, node, m_index_instances,  index_cache);


        func(node->value());

        std::array<bool, num_indices> indicies_to_modify{};

        foreach_index([]<int I>(node_type* node, typename nth_index<I>::type& instance, auto& modify, const auto& cache) {
            modify = instance.erase_if_modified(node, cache);
         }, node, m_index_instances,  indicies_to_modify, index_cache);


        indices_hints_tuple index_hints;

        bool insertable = get_foreach_index([]<int I>(node_type* node, typename nth_index<I>::type& instance, const auto& modify, auto& hints) {
            if (modify) return instance.preinsert_node(node, hints) == nullptr;
            return true;
        }, node, m_index_instances,  indicies_to_modify, index_hints);

        if (insertable) {
            foreach_index([]<int I>(node_type* node, typename nth_index<I>::type& instance, const auto& modify, const auto& hints) {
                if (modify) instance.insert_node(node, hints);
            }, node, m_index_instances,  indicies_to_modify, index_hints);
            return true;
        } else {
            foreach_index([]<int I>(node_type* node, typename nth_index<I>::type& instance, const auto& modify) {
                if (!modify) instance.remove_node(node);
            }, node, m_index_instances,  indicies_to_modify);
            do_erase_cleanup(node);
            return false;
        }
    }

    void do_clear()
    {
        foreach_index([]<int I>(std::nullptr_t, typename nth_index<I>::type& instance) {
            instance.do_clear();
         }, nullptr, m_index_instances);

        auto* node = m_begin;
        while (node) {
            auto* to_delete = node;
            node = node->next();
            std::allocator_traits<node_allocator_type>::destroy(m_alloc, to_delete);
            std::allocator_traits<node_allocator_type>::deallocate(m_alloc, to_delete, 1);
        }
        m_begin = m_end = nullptr;
    }

    size_t get_size() const
    {
        return m_size;
    }

    bool get_empty() const
    {
        return m_size == 0;
    }

public:

    tmi(const allocator_type& alloc = {})
        : inherited_index(*this),
          m_index_instances(index_tuple_helper<std::make_index_sequence<num_indices>>::make_index_types(*this)),
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
