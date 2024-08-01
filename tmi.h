// Copyright (c) 2024 Cory Fields
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Parts of this file have been copied from LLVM's libc++ and modified
// to work here. Their license applies to those functions. They are intended
// to serve only as a proof of concept for the red-black tree and should be
// rewritten asap.

#ifndef TMI_H_
#define TMI_H_

#include "tminode.h"
#include "tminode_detail.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <tuple>
#include <vector>

namespace tmi {
using namespace detail;

template <typename T, typename Comparators, typename Hashers, typename Allocator = std::allocator<T>>
class tmi
{
    using allocator_type = Allocator;
    using comparator_types = Comparators::comparator_types;
    using hasher_types = Hashers::hasher_types;
    static constexpr int num_comparators = std::tuple_size_v<comparator_types>;
    static constexpr int num_hashers = std::tuple_size_v<hasher_types>;
    using node_type = tminode<T, num_comparators, num_hashers>;
    using node_allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<node_type>;
    using base_type = node_type::tminode_base;
    using Color = typename base_type::Color;
    using hash_buckets = std::vector<base_type*>;

    struct comparator_insert_hints {
        base_type* m_parent{nullptr};
        bool m_inserted_left{false};
    };
    using comparator_hints_array = std::array<comparator_insert_hints, num_comparators>;

    struct hasher_insert_hints {
        size_t m_hash{0};
        base_type** m_bucket{nullptr};
    };
    using hasher_hints_array = std::array<hasher_insert_hints, num_hashers>;

    static constexpr size_t first_hashes_resize = 2048;

    base_type m_roots;
    std::array<hash_buckets, num_hashers> m_buckets{hash_buckets{}};

    node_type* m_begin{nullptr};
    node_type* m_end{nullptr};
    size_t m_size{0};

    comparator_types m_comparators;
    hasher_types m_hashers;

    node_allocator_type m_alloc;


    template <int I = 0, class Callable, typename Node, typename... Args>
    static void foreach_hasher(Callable&& func, Node* node, Args&&... args)
    {
        if constexpr (num_hashers) {
            func.template operator()<I>(node, std::get<I>(args)...);
        }
        if constexpr (I + 1 < num_hashers) {
            foreach_hasher<I + 1>(std::forward<Callable>(func), node, std::forward<Args>(args)...);
        }
    }

    template <int I = 0, class Callable, typename Node, typename... Args>
    static bool get_foreach_hasher(Callable&& func, Node* node, Args&&... args)
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
    static void foreach_comparator(Callable&& func, Node* node, Args&&... args)
    {
        if (num_comparators) {
            func.template operator()<I>(node, std::get<I>(args)...);
        }
        if constexpr (I + 1 < num_comparators) {
            foreach_comparator<I + 1>(std::forward<Callable>(func), node, std::forward<Args>(args)...);
        }
    }

    template <int I = 0, class Callable, typename Node, typename... Args>
    static bool get_foreach_comparator(Callable&& func, Node* node, Args&&... args)
    {
        if (num_comparators) {
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
    const auto& get_comparator() const
    {
        return std::get<I>(m_comparators);
    }

    template <int I>
    const auto& get_hasher() const
    {
        return std::get<I>(m_hashers);
    }

    template <int I>
    auto& get_buckets()
    {
        return std::get<I>(m_buckets);
    }

    template <int I>
    const auto& get_buckets() const
    {
        return std::get<I>(m_buckets);
    }

    template <int I>
    base_type* get_root_base() const
    {
        return m_roots.template left<I>();
    }

    template <int I>
    void set_root_node(node_type* node)
    {
        m_roots.template set_left<I>(node->get_base());
    }

    /*

    The below insert/erase impls were copied from libc++

    Their base type inheritance was dropped in order to come closer to
    providing a standard layout. Keeping T as he first member allows us to
    convert Between T* and node_type*. This is the trick that makes iterator_to
    work.

    Their root node trick is borrowed as well. The following is adapted from
    their comment:

//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

    The algorithms taking base_type* are red black tree algorithms.  Those
    algorithms taking a parameter named root should assume that root
    points to a proper red black tree (unless otherwise specified).

    Each algorithm herein assumes that root->m_parent points to a non-null
    structure which has a member m_left which points back to root.  No other
    member is read or written to at root->m_parent.

    root->m_parent_ will be referred to below (in comments only) as m_roots[I].
    m_roots[I]->m_left is an externably accessible lvalue for root, and can be
    changed by node insertion and removal (without explicit reference to
    m_roots[I]).

    All nodes (with the exception of m_roots[I]), even the node referred to as
    root, have a non-null m_parent field.

*/
    template <int I>
    static base_type* tree_max(base_type* x) {
      while (x->template right<I>() != nullptr)
        x = x->template right<I>();
      return x;
    }

    template <int I>
    static bool tree_is_left_child(base_type* x)
    {
        return x == x->template parent<I>()->template left<I>();
    }

    template <int I>
    static base_type* tree_min(base_type* x)
    {
        while (x->template left<I>() != nullptr)
            x = x->template left<I>();
        return x;
    }

    template <int I>
    static base_type* tree_next(base_type* x)
    {
        if (x->template right<I>() != nullptr)
            return tree_min<I>(x->template right<I>());
        while (!tree_is_left_child<I>(x))
            x = x->template parent<I>();
        return x->template parent<I>();
    }

    template <int I>
    static base_type* tree_prev(base_type* x) {
      if (x->template left<I>() != nullptr)
        return tree_max<I>(x->template left<I>());
      while (tree_is_left_child<I>(x))
        x = x->template parent<I>();
      return x->template parent<I>();
    }

    template <int I>
    static void tree_left_rotate(base_type* x)
    {
        base_type* y = x->template right<I>();
        x->template set_right<I>(y->template left<I>());
        if (x->template right<I>() != nullptr)
            x->template right<I>()->template set_parent<I>(x);
        y->template set_parent<I>(x->template parent<I>());
        if (tree_is_left_child<I>(x))
            x->template parent<I>()->template set_left<I>(y);
        else
            x->template parent<I>()->template set_right<I>(y);
        y->template set_left<I>(x);
        x->template set_parent<I>(y);
    }

    template <int I>
    static void tree_right_rotate(base_type* x)
    {
        base_type* y = x->template left<I>();
        x->template set_left<I>(y->template right<I>());
        if (x->template left<I>() != nullptr)
            x->template left<I>()->template set_parent<I>(x);
        y->template set_parent<I>(x->template parent<I>());
        if (tree_is_left_child<I>(x))
            x->template parent<I>()->template set_left<I>(y);
        else
            x->template parent<I>()->template set_right<I>(y);
        y->template set_right<I>(x);
        x->template set_parent<I>(y);
    }

    template <int I>
    static base_type* avl_rotate_right(base_type* x) {
        base_type* z = right<I>(x);
        base_type* t23 = left<I>(z);
        set_right<I>(x, t23);
        if (t23 != nullptr)
            set_parent<I>(t23, x);
        set_left<I>(z, x);
        set_parent<I>(x, z);
        if (balance_factor<I>(z) == 0) {
            set_balance_factor<I>(x, 1);
            set_balance_factor<I>(z, -1);
        } else {
            set_balance_factor<I>(x, 0);
            set_balance_factor<I>(z, 0);
        }
        return z;
    }

    template <int I>
    static base_type* avl_rotate_left(base_type* x) {
        base_type* z = left<I>(x);
        base_type* t23 = right<I>(z);
        set_right<I>(x, t23);
        if (t23 != nullptr)
            set_parent<I>(t23, x);
        set_right<I>(z, x);
        set_parent<I>(x, z);
        if (balance_factor<I>(z) == 0) {
            set_balance_factor<I>(x, 1);
            set_balance_factor<I>(z, -1);
        } else {
            set_balance_factor<I>(x, 0);
            set_balance_factor<I>(z, 0);
        }
        return z;
    }

    template <int I>
    static base_type* avl_rotate_rightleft(base_type* x)
    {
        base_type* z = right<I>(x);
        base_type* y = left<I>(z);
        base_type* t3 = right<I>(y);
        set_left<I>(z, t3);
        if (t3 != nullptr) {
            set_parent<I>(t3, z);
        }
        set_right<I>(y, z);
        set_parent<I>(z, y);
        base_type* t2 = left<I>(y);
        set_right<I>(x, t2);
        if (t2 != nullptr) {
            set_parent<I>(t2, x);
        }
        set_left<I>(y, x);
        set_parent<I>(x, y);

        if (balance_factor<I>(y) == 0) {
            set_balance_factor<I>(x, 0);
            set_balance_factor<I>(z, 0);
        } else if (balance_factor<I>(y) > 0) {
            set_balance_factor<I>(x, -1);
            set_balance_factor<I>(z, 0);
        } else {
            set_balance_factor<I>(x, 0);
            set_balance_factor<I>(z, 1);
        }
        set_balance_factor<I>(y, 0);
        return y;
    }

    template <int I>
    static base_type* avl_rotate_leftright(base_type* x)
    {
        base_type* z = left<I>(x);
        base_type* y = right<I>(z);
        base_type* t3 = left<I>(y);
        set_right<I>(z, t3);
        if (t3 != nullptr) {
            set_parent<I>(t3, z);
        }
        set_left<I>(y, z);
        set_parent<I>(z, y);
        base_type* t2 = right<I>(y);
        set_left<I>(x, t2);
        if (t2 != nullptr) {
            set_parent<I>(t2, x);
        }
        set_right<I>(y, x);
        set_parent<I>(x, y);

        if (balance_factor<I>(y) == 0) {
            set_balance_factor<I>(x, 0);
            set_balance_factor<I>(z, 0);
        } else if (balance_factor<I>(y) > 0) {
            set_balance_factor<I>(x, -1);
            set_balance_factor<I>(z, 0);
        } else {
            set_balance_factor<I>(x, 0);
            set_balance_factor<I>(z, -1);
        }
        set_balance_factor<I>(y, 0);
        return y;
    }

    template <int I>
    static void  avl_rebalance_after_insert(base_type* z, base_type*& new_root)
    {
        base_type* g;
        base_type* n;
        while (parent<I>(z)) {
            base_type* x = parent<I>(z);
            if (z == right<I>(x)) {
                if (balance_factor<I>(x) > 0) {
                    g = parent<I>(x);
                    if (balance_factor<I>(z) < 0) {
                        n = avl_rotate_rightleft<I>(x);
                    } else {
                        n = avl_rotate_left<I>(x);
                    }
                } else {
                    if (balance_factor<I>(x) < 0) {
                        set_balance_factor<I>(x, 0);
                        break;
                    }
                    set_balance_factor<I>(x, 1);
                    z = x;
                    continue;
                }
            } else {
                if (balance_factor<I>(x) < 0) {
                    g = parent<I>(x);
                    if (balance_factor<I>(z) > 0) {
                        n = avl_rotate_leftright<I>(x);
                    } else {
                        n = avl_rotate_right<I>(x);
                    }
                } else {
                    if (balance_factor<I>(x) > 0) {
                        set_balance_factor<I>(x, 0);
                        break;
                    }
                    set_balance_factor<I>(x, -1);
                    z = x;
                    continue;
                }
            }
            set_parent<I>(n, g);
            if (g != nullptr) {
                if (x == left<I>(g)) {
                    set_left<I>(g, n);
                } else {
                    set_right<I>(g, n);
                }
            } else {
                new_root = n;
            }
            break;
        }
    }




    // Precondition:  root != nullptr && z != nullptr.
    //                tree_invariant(root) == true.
    //                z == root or == a direct or indirect child of root.
    // Effects:  unlinks z from the tree rooted at root, rebalancing as needed.
    // Postcondition: tree_invariant(end_node->template left<I>()) == true && end_node->template left<I>()
    //                nor any of its children refer to z.  end_node->template left<I>()
    //                may be different than the value passed in as root.
    template <int I>
    static void tree_remove(base_type* root, base_type* z)
    {
        assert(root);
        assert(z);
        // z will be removed from the tree.  Client still needs to destruct/deallocate it
        // y is either z, or if z has two children, tree_next(z).
        // y will have at most one child.
        // y will be the initial hole in the tree (make the hole at a leaf)
        base_type* y = (z->template left<I>() == nullptr || z->template right<I>() == nullptr) ?
                        z : tree_next<I>(z);
        // x is y's possibly null single child
        base_type* x = y->template left<I>() != nullptr ? y->template left<I>() : y->template right<I>();
        // w is x's possibly null uncle (will become x's sibling)
        base_type* w = nullptr;
        // link x to y's parent, and find w
        if (x != nullptr)
            x->template set_parent<I>(y->template parent<I>());
        if (tree_is_left_child<I>(y))
        {
            y->template parent<I>()->template set_left<I>(x);
            if (y != root)
                w = y->template parent<I>()->template right<I>();
            else
                root = x;  // w == nullptr
        }
        else
        {
            y->template parent<I>()->template set_right<I>(x);
            // y can't be root if it is a right child
            w = y->template parent<I>()->template left<I>();
        }
        bool removed_black = y->template color<I>() == Color::BLACK;
        // If we didn't remove z, do so now by splicing in y for z,
        //    but copy z's color.  This does not impact x or w.
        if (y != z)
        {
            // z->template left<I>() != nulptr but z->template right<I>() might == x == nullptr
            y->template set_parent<I>(z->template parent<I>());
            if (tree_is_left_child<I>(z))
                y->template parent<I>()->template set_left<I>(y);
            else
                y->template parent<I>()->template set_right<I>(y);
            y->template set_left<I>(z->template left<I>());
            y->template left<I>()->template set_parent<I>(y);
            y->template set_right<I>(z->template right<I>());
            if (y->template right<I>() != nullptr)
                y->template right<I>()->template set_parent<I>(y);
            y->template set_color<I>(z->template color<I>());
            if (root == z)
                root = y;
        }
        if (removed_black && root != nullptr)
        {
            // Rebalance:
            // x has an implicit black color (transferred from the removed y)
            //    associated with it, no matter what its color is.
            // If x is root (in which case it can't be null), it is supposed
            //    to be black anyway, and if it is doubly black, then the double
            //    can just be ignored.
            // If x is red (in which case it can't be null), then it can absorb
            //    the implicit black just by setting its color to black.
            // Since y was black and only had one child (which x points to), x
            //   is either red with no children, else null, otherwise y would have
            //   different black heights under left and right pointers.
            // if (x == root || x != nullptr && !x->is_black_)
            if (x != nullptr)
                x->template set_color<I>(Color::BLACK);
            else {
                //  Else x isn't root, and is "doubly black", even though it may
                //     be null.  w can not be null here, else the parent would
                //     see a black height >= 2 on the x side and a black height
                //     of 1 on the w side (w must be a non-null black or a red
                //     with a non-null black child).
                fixup_after_remove<I>(root, w);
            }
        }
    }

    template <int I>
    static void tree_balance_after_insert(base_type* root, base_type* x)
    {
        x->template set_color<I>(x == root ? Color::BLACK : Color::RED);
        while (x != root && x->template parent<I>()->template color<I>() == Color::RED)
        {
            // x->template parent<I>() != root because x->template parent<I>()->is_black == false
            if (tree_is_left_child<I>(x->template parent<I>()))
            {
                base_type* y = x->template parent<I>()->template parent<I>()->template right<I>();
                if (y != nullptr && y->template color<I>() == Color::RED)
                {
                    x = x->template parent<I>();
                    x->template set_color<I>(Color::BLACK);
                    x = x->template parent<I>();
                    x->template set_color<I>(x == root ? Color::BLACK : Color::RED);
                    y->template set_color<I>(Color::BLACK);
                }
                else
                {
                    if (!tree_is_left_child<I>(x))
                    {
                        x = x->template parent<I>();
                        tree_left_rotate<I>(x);
                    }
                    x = x->template parent<I>();
                    x->template set_color<I>(Color::BLACK);
                    x = x->template parent<I>();
                    x->template set_color<I>(Color::RED);
                    tree_right_rotate<I>(x);
                    break;
                }
            }
            else
            {
                base_type* y = x->template parent<I>()->template parent<I>()->template left<I>();
                if (y != nullptr && y->template color<I>() == Color::RED)
                {
                    x = x->template parent<I>();
                    x->template set_color<I>(Color::BLACK);
                    x = x->template parent<I>();
                    x->template set_color<I>(x == root ? Color::BLACK : Color::RED);
                    y->template set_color<I>(Color::BLACK);
                }
                else
                {
                    if (tree_is_left_child<I>(x))
                    {
                        x = x->template parent<I>();
                        tree_right_rotate<I>(x);
                    }
                    x = x->template parent<I>();
                    x->template set_color<I>(Color::BLACK);
                    x = x->template parent<I>();
                    x->template set_color<I>(Color::RED);
                    tree_left_rotate<I>(x);
                    break;
                }
            }
        }
    }

    template <int I>
    static void fixup_after_remove(base_type* root, base_type* w)
    {
        base_type* x = nullptr;
        while (true)
        {
            if (!tree_is_left_child<I>(w))  // if x is left child
            {
                if (w->template color<I>() == Color::RED)
                {
                    w->template set_color<I>(Color::BLACK);
                    w->template parent<I>()->template set_color<I>(Color::RED);
                    tree_left_rotate<I>(w->template parent<I>());
                    // x is still valid
                    // reset root only if necessary
                    if (root == w->template left<I>())
                        root = w;
                    // reset sibling, and it still can't be null
                    w = w->template left<I>()->template right<I>();
                }
                // w->is_black_ is now true, w may have null children
                if ((w->template left<I>()  == nullptr || w->template left<I>()->template color<I>() == Color::BLACK) &&
                    (w->template right<I>() == nullptr || w->template right<I>()->template color<I>() == Color::BLACK))
                {
                    w->template set_color<I>(Color::RED);
                    x = w->template parent<I>();
                    // x can no longer be null
                    if (x == root || x->template color<I>() == Color::RED)
                    {
                        x->template set_color<I>(Color::BLACK);
                        break;
                    }
                    // reset sibling, and it still can't be null
                    w = tree_is_left_child<I>(x) ?
                                x->template parent<I>()->template right<I>() :
                                x->template parent<I>()->template left<I>();
                    // continue;
                }
                else  // w has a red child
                {
                    if (w->template right<I>() == nullptr || w->template right<I>()->template color<I>() == Color::BLACK)
                    {
                        // w left child is non-null and red
                        w->template left<I>()->template set_color<I>(Color::BLACK);
                        w->template set_color<I>(Color::RED);
                        tree_right_rotate<I>(w);
                        // w is known not to be root, so root hasn't changed
                        // reset sibling, and it still can't be null
                        w = w->template parent<I>();
                    }
                    // w has a right red child, left child may be null
                    w->template set_color<I>(w->template parent<I>()->template color<I>());
                    w->template parent<I>()->template set_color<I>(Color::BLACK);
                    w->template right<I>()->template set_color<I>(Color::BLACK);
                    tree_left_rotate<I>(w->template parent<I>());
                    break;
                }
            }
            else
            {
                if (w->template color<I>() == Color::RED)
                {
                    w->template set_color<I>(Color::BLACK);
                    w->template parent<I>()->template set_color<I>(Color::RED);
                    tree_right_rotate<I>(w->template parent<I>());
                    // x is still valid
                    // reset root only if necessary
                    if (root == w->template right<I>())
                        root = w;
                    // reset sibling, and it still can't be null
                    w = w->template right<I>()->template left<I>();
                }
                // w->is_black_ is now true, w may have null children
                if ((w->template left<I>()  == nullptr || w->template left<I>()->template color<I>() == Color::BLACK) &&
                    (w->template right<I>() == nullptr || w->template right<I>()->template color<I>() == Color::BLACK))
                {
                    w->template set_color<I>(Color::RED);
                    x = w->template parent<I>();
                    // x can no longer be null
                    if (x->template color<I>() == Color::RED || x == root)
                    {
                        x->template set_color<I>(Color::BLACK);
                        break;
                    }
                    // reset sibling, and it still can't be null
                    w = tree_is_left_child<I>(x) ?
                                x->template parent<I>()->template right<I>() :
                                x->template parent<I>()->template left<I>();
                    // continue;
                }
                else  // w has a red child
                {
                    if (w->template left<I>() == nullptr || w->template left<I>()->template color<I>() == Color::BLACK)
                    {
                        // w right child is non-null and red
                        w->template right<I>()->template set_color<I>(Color::BLACK);
                        w->template set_color<I>(Color::RED);
                        tree_left_rotate<I>(w);
                        // w is known not to be root, so root hasn't changed
                        // reset sibling, and it still can't be null
                        w = w->template parent<I>();
                    }
                    // w has a left red child, right child may be null
                    w->template set_color<I>(w->template parent<I>()->template color<I>());
                    w->template parent<I>()->template set_color<I>(Color::BLACK);
                    w->template left<I>()->template set_color<I>(Color::BLACK);
                    tree_right_rotate<I>(w->template parent<I>());
                    break;
                }
            }
        }
    }

//===----------------------------------------------------------------------===//
//
//   LLVM Code ends here
//
//===----------------------------------------------------------------------===//

    /*
        Replace the current head with the new node and set the
        node's next pointer to what previously occupied the head
    */

    template <int I>
    void insert_in_buckets_unchecked(hash_buckets& buckets, base_type* node)
    {
        size_t index = node->template hash<I>() % buckets.size();
        base_type*& bucket = buckets.at(index);
        node->template set_next_hashptr<I>(bucket);
        bucket = node;
    }

    /*
        Create new buckets, iterate through the old ones moving them to
        their updated indicies in the new buckets, then replace the old
        with the new
    */
    template <int I>
    void rehash(size_t new_size)
    {
        hash_buckets& buckets = get_buckets<I>();
        hash_buckets new_buckets(new_size, nullptr);
        for (base_type* bucket : buckets) {
            base_type* cur_node = bucket;
            while (cur_node) {
                base_type* next_node = cur_node->template next_hash<I>();
                insert_in_buckets_unchecked<I>(new_buckets, cur_node);
                cur_node = next_node;
            }
        }
        buckets = std::move(new_buckets);
    }

    template <int I>
    void hash_remove_direct(base_type* node)
    {
        const auto& hasher = get_hasher<I>();
        hash_buckets& buckets = get_buckets<I>();
        auto hash = node->template hash<I>();
        size_t bucket_count = buckets.size();
        if (!bucket_count) {
            return;
        }
        auto index = hash % bucket_count;

        base_type*& bucket = buckets.at(index);
        base_type* cur_node = bucket;
        base_type* prev_node = cur_node;
        while (cur_node) {
            if (cur_node->template hash<I>() == hash && hasher(cur_node->node()->value(), node->node()->value())) {
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

    /*
        First rehash if necessary, using first_hashes_resize as the initial
        size if empty. Then find calculate the bucket and insert there.
    */
    template <int I>
    bool preinsert_node_hash(node_type* node, hasher_insert_hints& hints)
    {
        const auto& hasher = get_hasher<I>();
        hash_buckets& buckets = get_buckets<I>();
        size_t bucket_count = buckets.size();
        auto hash = hasher(node->value());

        if (!bucket_count) {
            buckets.resize(first_hashes_resize, nullptr);
            bucket_count = first_hashes_resize;
        } else if (static_cast<double>(m_size) / static_cast<double>(bucket_count) >= 0.8) {
            bucket_count *= 2;
            rehash<I>(bucket_count);
        }

        size_t index = hash % buckets.size();
        base_type*& bucket = buckets.at(index);

        if constexpr (std::tuple_element_t<I, hasher_types>::hashed_unique()) {
            base_type* curr = bucket;
            base_type* prev = curr;
            while (curr) {
                if (hasher(curr->node()->value(), node->value())) {
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

    template <int I>
    void insert_node_hash(node_type* node, const hasher_insert_hints& hints)
    {
        base_type* node_base = node->get_base();
        node_base->template set_hash<I>(hints.m_hash);
        node_base->template set_next_hashptr<I>(*hints.m_bucket);
        *hints.m_bucket = node_base;
    }


    template <int I, typename H>
    node_type* find_hash(const H::hash_type& hash_key) const
    {
        const auto& hasher = get_hasher<I>();
        size_t hash = hasher(hash_key);
        const hash_buckets& buckets = get_buckets<I>();
        size_t bucket_count = buckets.size();
        if (!bucket_count) {
            return nullptr;
        }
        auto* node = buckets.at(hash % bucket_count);
        while (node) {
            if (node->template hash<I>() == hash) {
                if (hasher(node->node()->value(), hash_key)) {
                    return node->node();
                }
            }
            node = node->template next_hash<I>();
        }
        return nullptr;
    }

    template <int I, typename H>
    node_type* do_find(const H::hash_type& hash_key) const
    {
        if constexpr (std::is_same_v<H, typename std::tuple_element_t<I, hasher_types>>) {
            return find_hash<I, H>(hash_key);
        } else if constexpr (I + 1 < num_hashers) {
            return do_find<I + 1, H>(hash_key);
        } else {
            // g++ isn't able to cope with this static_assert for some reason.
            // This should be unreachable code.
            //static_assert(false, "Invalid hasher");
            return nullptr;
        }
    }

    template <int I>
    bool preinsert_node_comparator(node_type* node, comparator_insert_hints& hints)
    {
        const auto& comparator = get_comparator<I>();
        base_type* parent = nullptr;
        base_type* curr = get_root_base<I>();

        bool inserted_left = false;
        while (curr != nullptr) {
            parent = curr;

            if constexpr (std::tuple_element_t<I, comparator_types>::sorted_unique()) {
                if (comparator(node->value(), curr->node()->value())) {
                    curr = curr->template left<I>();
                    inserted_left = true;
                } else if (comparator(curr->node()->value(), node->value())) {
                    curr = curr->template right<I>();
                    inserted_left = false;
                } else {
                    return false;
                }
            } else {
                if (comparator(node->value(), curr->node()->value())) {
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
        return true;
    }

    template <int I>
    void insert_node_comparator(node_type* node, const comparator_insert_hints& hints)
    {
        base_type* base = node->get_base();
        base_type* parent = hints.m_parent;
        if (!parent) {
            set_root_node<I>(node);
            base->template set_parent<I>(&m_roots);
        } else if (hints.m_inserted_left) {
            base->template set_parent<I>(parent);
            parent->template set_left<I>(base);
        } else {
            base->template set_parent<I>(parent);
            parent->template set_right<I>(base);
        }
        //tree_balance_after_insert<I>(get_root_base<I>(), base);
        base_type* new_root = nullptr;
        avl_rebalance_after_insert<I>(base, new_root);
        if (new_root) {
            m_roots.template set_left<I>(new_root);
        }
    }

    bool do_insert(node_type* node)
    {
        comparator_hints_array comp_hints;
        hasher_hints_array hash_hints;
        bool can_insert;

        can_insert = get_foreach_hasher([this]<int I>(node_type* node, auto& hints) {
            if (!preinsert_node_hash<I>(node, hints)) return false;
            return true;
        }, node, hash_hints);

        if (!can_insert) return false;

        can_insert = get_foreach_comparator([this]<int I>(node_type* node, auto& hints) {
            if (!preinsert_node_comparator<I>(node, hints)) return false;
            return true;
        }, node, comp_hints);

        if (!can_insert) return false;

        foreach_hasher([this]<int I>(node_type* node, auto& hints) {
            insert_node_hash<I>(node, hints);
        }, node, hash_hints);

        foreach_comparator([this]<int I>(node_type* node, auto& hints) {
            insert_node_comparator<I>(node, hints);
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
            tree_remove<I>(get_root_base<I>(), node->get_base());
        }, node);

        foreach_hasher([this]<int I>(node_type* node) {
            hash_remove_direct<I>(node->get_base());
        }, node);
        do_erase_cleanup(node);
    }

    template <typename Callable>
    bool do_modify(node_type* node, Callable&& func)
    {
        struct hasher_premodify_cache {
            bool m_is_head{false};
            base_type** m_bucket{nullptr};
            base_type* m_prev{nullptr};
        };
        struct hash_modify_actions {
            bool m_do_reinsert{false};
        };
        struct comparator_modify_actions {
            bool m_do_reinsert{false};
        };

        using hasher_premodify_cache_array = std::array<hasher_premodify_cache, num_hashers>;
        using hasher_modify_actions_array = std::array<hash_modify_actions, num_hashers>;
        using comparator_modify_actions_array = std::array<comparator_modify_actions, num_comparators>;


        // Create a cache of the pre-modified hash buckets
        hasher_premodify_cache_array hash_cache;
        foreach_hasher([this]<int I>(node_type* node, auto& cache, const auto& hasher, auto& buckets) {
            base_type* base = node->get_base();
            auto hash = base->template hash<I>();
            size_t bucket_count = buckets.size();
            if (!bucket_count) {
                return;
            }
            auto index = hash % bucket_count;

            base_type*& bucket = buckets.at(index);
            base_type* cur_node = bucket;
            base_type* prev_node = cur_node;
            while (cur_node) {
                if (cur_node->template hash<I>() == hash && hasher(cur_node->node()->value(), node->value())) {
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
        }, node, hash_cache, m_hashers, m_buckets);


        func(node->value());

        hasher_modify_actions_array hash_modify;
        comparator_modify_actions_array comp_modify;

        // Erase modified hashes
        foreach_hasher([this]<int I>(node_type* node, auto& modify, const auto& cache, const auto& hasher) {
            base_type* base = node->get_base();
            if (hasher(node->value()) != base->template hash<I>()) {
                if (cache.m_is_head) {
                    *cache.m_bucket = nullptr;
                } else {
                    cache.m_prev->template set_next_hashptr<I>(base->template next_hash<I>());
                }
                modify.m_do_reinsert = true;
            }
         }, node, hash_modify, hash_cache, m_hashers);


        // Erase modified sorts
        foreach_comparator([this]<int I>(node_type* node, auto& modify, const auto& comparator) {
            base_type* base = node->get_base();
            base_type* next_ptr = nullptr;
            base_type* prev_ptr = nullptr;
            base_type* root = get_root_base<I>();

            if (base != tree_min<I>(root))
                prev_ptr = tree_prev<I>(base);
            if (base != tree_max<I>(root))
                next_ptr = tree_next<I>(base);

            bool needs_resort = ((next_ptr != nullptr && comparator(next_ptr->node()->value(), node->value())) ||
                                 (prev_ptr != nullptr && comparator(node->value(), prev_ptr->node()->value())));
            if (needs_resort) {
                tree_remove<I>(root, base);
                base->template set_parent<I>(nullptr);
                base->template set_left<I>(nullptr);
                base->template set_right<I>(nullptr);
                base->template set_color<I>(Color::RED);
                modify.m_do_reinsert = true;
            }
        }, node, comp_modify, m_comparators);

        // At this point the node has been removed from all buckets and trees.
        // Test to see if it's reinsertable everywhere or delete it.

        comparator_hints_array comp_hints;
        hasher_hints_array hash_hints;

        // Check to see if any new hashes can be safely inserted
        bool insertable = get_foreach_hasher([this]<int I>(node_type* node, const auto& modify, auto& hints) {
            if (modify.m_do_reinsert) return preinsert_node_hash<I>(node, hints);
            return true;
        }, node, hash_modify, hash_hints);

        // Check to see if any new sorts can be safely inserted
        if (insertable) {
            insertable = get_foreach_comparator([this]<int I>(node_type* node, const auto& modify, auto& hints) {
                if (modify.m_do_reinsert) return preinsert_node_comparator<I>(node, hints);
                return true;
            }, node, comp_modify, comp_hints);
        }

        if (!insertable) {
            do_erase_cleanup(node);
            return false;
        }

        foreach_hasher([this]<int I>(node_type* node, const auto& modify, const auto& hints) {
            if (modify.m_do_reinsert) insert_node_hash<I>(node, hints);
            return true;
        },
                       node, hash_modify, hash_hints);

        foreach_comparator([this]<int I>(node_type* node, const auto& modify, const auto& hints) {
            if (modify.m_do_reinsert) insert_node_comparator<I>(node, hints);
            return true;
        },
                           node, comp_modify, comp_hints);

        return true;
    }


public:
    class iterator;
    template <int I>
    class sort_iterator
    {
        node_type* m_node{};

    public:
        friend class iterator;
        friend class tmi;
        typedef T value_type;
        typedef T* pointer;
        typedef T& reference;
        using element_type = T;
        sort_iterator() = default;
        sort_iterator(node_type* node) : m_node(node) {}
        T& operator*() const { return m_node->value(); }
        T* operator->() const { return &m_node->value(); }
        sort_iterator& operator++()
        {
            m_node = tree_next<I>(m_node->get_base())->node();
            return *this;
        }
        sort_iterator& operator--()
        {
            m_node = tree_prev<I>(m_node->get_base())->node();
            return *this;
        }
        sort_iterator operator++(int)
        {
            sort_iterator copy(m_node);
            ++(*this);
            return copy;
        }
        sort_iterator operator--(int)
        {
            sort_iterator copy(m_node);
            --(*this);
            return copy;
        }
        bool operator==(sort_iterator rhs) const { return m_node == rhs.m_node; }
        bool operator!=(sort_iterator rhs) const { return m_node != rhs.m_node; }
    };

    template <int I>
    class const_sort_iterator
    {
        const node_type* m_node{};

    public:
        typedef const T value_type;
        typedef const T* pointer;
        typedef const T& reference;
        using element_type = const T;
        const_sort_iterator() = default;
        const_sort_iterator(const node_type* node) : m_node(node) {}
        const T& operator*() const { return m_node->value(); }
        const T* operator->() const { return &m_node->value(); }
        const_sort_iterator& operator++()
        {
            m_node = tree_next<I>(m_node->get_base())->m_node();
            return *this;
        }
        const_sort_iterator& operator--()
        {
            m_node = tree_prev<I>(m_node->get_base())->node();
            return *this;
        }
        const_sort_iterator operator++(int)
        {
            const_sort_iterator copy(m_node);
            ++(*this);
            return copy;
        }
        const_sort_iterator operator--(int)
        {
            const_sort_iterator copy(m_node);
            --(*this);
            return copy;
        }
        bool operator==(const_sort_iterator rhs) const { return m_node == rhs.m_node; }
        bool operator!=(const_sort_iterator rhs) const { return m_node != rhs.m_node; }
    };

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


    tmi() = default;

    tmi(const allocator_type& alloc) : m_alloc(alloc) {}

    ~tmi()
    {
        clear();
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
        m_roots = {};
        for (auto& buckets : m_buckets) {
            buckets.clear();
        }
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
        base_type* root = get_root_base<I>();
        if (root == nullptr)
            return sort_iterator<I>(nullptr);
        return sort_iterator<I>(tree_min<I>(root)->node());
    }

    template <int I>
    sort_iterator<I> sort_end() const
    {
        return sort_iterator<I>(nullptr);
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
};

} // namespace tmi

#endif // TMI_H_
