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
    using parent_type = tmi<T, Comparators, Hashers, Allocator>;
    using allocator_type = Allocator;
    using comparator_types = Comparators::comparator_types;
    using hasher_types = Hashers::hasher_types;
    static constexpr int num_comparators = std::tuple_size_v<comparator_types>;
    static constexpr int num_hashers = std::tuple_size_v<hasher_types>;
    using node_type = tminode<T, num_comparators, num_hashers>;
    using node_allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<node_type>;
    using base_type = node_type::base_type;
    static_assert(num_comparators > 0 || num_hashers > 0, "No hashers or comparators defined");

    template <int I>
    using tmi_comparator_type = tmi_comparator<T, num_comparators, num_hashers, I, std::tuple_element_t<I, comparator_types>, parent_type>;

    template <int I>
    using tmi_hasher_type = tmi_hasher<T, num_comparators, num_hashers, I, std::tuple_element_t<I, hasher_types>, parent_type>;

    template <typename>
    struct index_helper;
    template <size_t... ints>
    struct index_helper<std::index_sequence<ints...>> {
        using hasher_types = std::tuple< tmi_hasher_type<ints> ...>;
        using hasher_hints = std::tuple< typename tmi_hasher_type<ints>::insert_hints ...>;
        using hasher_premodify_cache = std::tuple< typename tmi_hasher_type<ints>::premodify_cache ...>;
        using comparator_types = std::tuple< tmi_comparator_type<ints> ...>;
        using comparator_hints = std::tuple< typename tmi_comparator_type<ints>::insert_hints ...>;
        using comparator_premodify_cache = std::tuple< typename tmi_comparator_type<ints>::premodify_cache ...>;

        static hasher_types make_hasher_types(parent_type& parent) {
            return std::make_tuple( tmi_hasher_type<ints>(parent) ...);
        }
        static comparator_types make_comparator_types(parent_type& parent) {
            return std::make_tuple( tmi_comparator_type<ints>(parent) ...);
        }

    };
    using hashers_tuple = index_helper<std::make_index_sequence<num_hashers>>::hasher_types;
    using hashers_hints_tuple = index_helper<std::make_index_sequence<num_hashers>>::hasher_hints;
    using hashers_premodify_cache_tuple = index_helper<std::make_index_sequence<num_hashers>>::hasher_premodify_cache;
    using comparators_tuple = index_helper<std::make_index_sequence<num_hashers>>::comparator_types;
    using comparators_hints_tuple = index_helper<std::make_index_sequence<num_comparators>>::comparator_hints;
    using comparators_premodify_cache_tuple = index_helper<std::make_index_sequence<num_comparators>>::comparator_premodify_cache;


    template <int I>
    using sort_iterator = tmi_comparator_type<I>::iterator;

    template <int I>
    using hash_iterator = tmi_hasher_type<I>::iterator;

    template <typename, int, int, int, typename, typename>
    friend class tmi_hasher;

    template <typename, int, int, int, typename, typename>
    friend class tmi_comparator;

private:
    node_type* m_begin{nullptr};
    node_type* m_end{nullptr};
    size_t m_size{0};

    comparators_tuple m_comparator_instances;
    hashers_tuple m_hasher_instances;

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
        return std::get<I>(m_hasher_instances);
    }

    template <int I>
    auto& get_hasher_instance()
    {
        return std::get<I>(m_hasher_instances);
    }

    template <int I>
    const auto& get_comparator_instance() const
    {
        return std::get<I>(m_comparator_instances);
    }

    template <int I>
    auto& get_comparator_instance()
    {
        return std::get<I>(m_comparator_instances);
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

    node_type* do_insert(node_type* node)
    {
        comparators_hints_tuple comp_hints;
        hashers_hints_tuple hash_hints;

        bool can_insert;
        node_type* conflict = nullptr;
        can_insert = get_foreach_hasher([this, &conflict]<int I>(node_type* node, auto& hints) {
            conflict = get_hasher_instance<I>().preinsert_node_hash(node, hints);
            return conflict == nullptr;
        }, node, hash_hints);

        if (!can_insert) return conflict;

        can_insert = get_foreach_comparator([this, &conflict]<int I>(node_type* node, auto& hints) {
            conflict = get_comparator_instance<I>().preinsert_node_comparator(node, hints);
            return conflict == nullptr;
        }, node, comp_hints);

        if (!can_insert) return conflict;

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
    std::pair<node_type*, bool> emplace(Args&&... args)
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

    void erase(node_type* node)
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
    bool modify(node_type* node, Callable&& func)
    {
        struct hash_modify_actions {
            bool m_do_reinsert{false};
        };
        struct comparator_modify_actions {
            bool m_do_reinsert{false};
        };

        using hasher_modify_actions_array = std::array<hash_modify_actions, num_hashers>;
        using comparator_modify_actions_array = std::array<comparator_modify_actions, num_comparators>;


        // Create a cache of the pre-modified hash buckets
        hashers_premodify_cache_tuple hash_cache;
        foreach_hasher([this]<int I>(node_type* node, auto& cache) {
            if constexpr (tmi_hasher_type<I>::requires_premodify_cache()) {
                get_hasher_instance<I>().hasher_create_premodify_cache(node, cache);
            }
        }, node, hash_cache);

        comparators_premodify_cache_tuple comp_cache;
        foreach_comparator([this]<int I>(node_type* node, auto& cache) {
            if constexpr (tmi_comparator_type<I>::requires_premodify_cache()) {
                get_comparator_instance<I>().comparator_create_premodify_cache(node, cache);
            }
        }, node, comp_cache);


        func(node->value());

        hasher_modify_actions_array hash_modify;
        comparator_modify_actions_array comp_modify;

        // Erase modified hashes
        foreach_hasher([this]<int I>(node_type* node, auto& modify, const auto& cache) {
            modify.m_do_reinsert = get_hasher_instance<I>().hasher_erase_if_modified(node, cache);
         }, node, hash_modify, hash_cache);


        // Erase modified sorts
        foreach_comparator([this]<int I>(node_type* node, auto& modify, const auto& cache) {
            modify.m_do_reinsert = get_comparator_instance<I>().comparator_erase_if_modified(node, cache);
        }, node, comp_modify, comp_cache);

        // At this point the node has been removed from all buckets and trees.
        // Test to see if it's reinsertable everywhere or delete it.

        comparators_hints_tuple comp_hints;
        hashers_hints_tuple hash_hints;

        // Check to see if any new hashes can be safely inserted
        bool insertable = get_foreach_hasher([this]<int I>(node_type* node, const auto& modify, auto& hints) {
            if (modify.m_do_reinsert) return get_hasher_instance<I>().preinsert_node_hash(node, hints) == nullptr;
            return true;
        }, node, hash_modify, hash_hints);

        // Check to see if any new sorts can be safely inserted
        if (insertable) {
            insertable = get_foreach_comparator([this]<int I>(node_type* node, const auto& modify, auto& hints) {
                if (modify.m_do_reinsert) return get_comparator_instance<I>().preinsert_node_comparator(node, hints) == nullptr;
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
    const auto& do_get_comparators() const
    {
        if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, comparator_types>>) {
            return get_comparator_instance<I>();
        }
        if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, comparator_types>::tag>) {
            return get_comparator_instance<I>();
        }
        else if constexpr (I + 1 < num_comparators) {
            return do_get_comparators<I + 1, Index>();
        }
    }

    template <int I = 0, typename Index>
    const auto& do_get_hashers() const
    {
        if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, hasher_types>>) {
            return get_hasher_instance<I>();
        }
        if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, hasher_types>::tag>) {
            return get_hasher_instance<I>();
        }
        else if constexpr (I + 1 < num_hashers) {
            return do_get_hashers<I + 1, Index>();
        }
    }


    template <int I = 0, typename Index>
    const auto& do_get() const
    {
        if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, comparator_types>>) {
            return get_comparator_instance<I>();
        } else if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, comparator_types>::tag>) {
            return get_comparator_instance<I>();
        } else if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, hasher_types>>) {
            return get_hasher_instance<I>();
        } else if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, hasher_types>::tag>) {
            return get_hasher_instance<I>();
        } else if constexpr (I + 1 < num_comparators && I + 1 < num_hashers) {
            return do_get<I + 1, Index>();
        } else if constexpr (I + 1 < num_comparators) {
            return do_get_comparators<I + 1, Index>();
        } else if constexpr (I + 1 < num_hashers) {
            return do_get_hashers<I + 1, Index>();
        }
    }


    template <int I = 0, typename Index>
    auto& do_get_comparators()
    {
        if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, comparator_types>>) {
            return get_comparator_instance<I>();
        }
        if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, comparator_types>::tag>) {
            return get_comparator_instance<I>();
        }
        else if constexpr (I + 1 < num_comparators) {
            return do_get_comparators<I + 1, Index>();
        }
    }

    template <int I = 0, typename Index>
    auto& do_get_hashers()
    {
        if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, hasher_types>>) {
            return get_hasher_instance<I>();
        }
        if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, hasher_types>::tag>) {
            return get_hasher_instance<I>();
        }
        else if constexpr (I + 1 < num_hashers) {
            return do_get_hashers<I + 1, Index>();
        }
    }


    template <int I = 0, typename Index>
    auto& do_get()
    {
        if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, comparator_types>>) {
            return get_comparator_instance<I>();
        } else if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, comparator_types>::tag>) {
            return get_comparator_instance<I>();
        } else if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, hasher_types>>) {
            return get_hasher_instance<I>();
        } else if constexpr (std::is_same_v<Index, typename std::tuple_element_t<I, hasher_types>::tag>) {
            return get_hasher_instance<I>();
        } else if constexpr (I + 1 < num_comparators && I + 1 < num_hashers) {
            return do_get<I + 1, Index>();
        } else if constexpr (I + 1 < num_comparators) {
            return do_get_comparators<I + 1, Index>();
        } else if constexpr (I + 1 < num_hashers) {
            return do_get_hashers<I + 1, Index>();
        }
    }
/*
    template <int I = 0, typename Index>
    auto& do_get()
    {
        if constexpr (I < num_comparators && std::is_same_v<Index, typename std::tuple_element_t<I, comparator_types>>) {
            return get_comparator_instance<I>();
        } else if constexpr (I < num_hashers && std::is_same_v<Index, typename std::tuple_element_t<I, hasher_types>>) {
            return get_hasher_instance<I>();
        } else if constexpr (I < num_hashers && std::is_same_v<Index, typename std::tuple_element_t<I, hasher_types>::tag>) {
            return get_hasher_instance<I>();
        } else if constexpr (I + 1 < num_comparators || I + 1 < num_hashers) {
            return do_get<I + 1, Index>();
        }
    }

    template <int I = 0, typename Index>
    const auto& do_get() const
    {
        if constexpr (I < num_comparators && std::is_same_v<Index, typename std::tuple_element_t<I, comparator_types>>) {
            return get_comparator_instance<I>();
        } else if constexpr (I < num_hashers && std::is_same_v<Index, typename std::tuple_element_t<I, hasher_types>>) {
            return get_hasher_instance<I>();
        } else if constexpr (I < num_hashers && std::is_same_v<Index, typename std::tuple_element_t<I, hasher_types>::tag>) {
            return get_hasher_instance<I>();
        } else if constexpr (I + 1 < num_comparators || I + 1 < num_hashers) {
            return do_get<I + 1, Index>();
        }
    }
*/
public:

    tmi(const allocator_type& alloc = {}) : m_comparator_instances(index_helper<std::make_index_sequence<num_comparators>>::make_comparator_types(*this)),
                                            m_hasher_instances(index_helper<std::make_index_sequence<num_hashers>>::make_hasher_types(*this)),
                                            m_alloc(alloc)
    {
    }

    ~tmi()
    {
        clear();
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

    size_t size() const { return m_size; }

    bool empty() const { return m_size == 0; }

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
