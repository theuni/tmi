#ifndef TMINODE_DETAIL_H
#define TMINODE_DETAIL_H

#include "tminode.h"

namespace tmi {

namespace detail {

    template <int I>
    void set_right(auto* lhs, auto* rhs)
    {
        lhs->template set_right<I>(rhs);
    }

    template <int I>
    void set_left(auto* lhs,auto* rhs)
    {
        lhs->template set_left<I>(rhs);
    }

    template <int I>
    auto* parent(auto* base)
    {
        return base->template parent<I>();
    }

    template <int I>
    void set_parent(auto* lhs, auto* rhs)
    {
        lhs->template set_parent<I>(rhs);
    }

    template <int I>
    auto color(auto* base)
    {
        return base->template color<I>();
    }

    template <int I>
    void set_color(auto* base, auto color)
    {
        base->template set_color<I>(color);
    }

    template <int I>
    auto* left(auto* base)
    {
        return base->template left<I>();
    }

    template <int I>
    auto* right(auto* base)
    {
        return base->template right<I>();
    }

    template <int I>
    auto* next_hash(auto* base)
    {
        return base->template next_hash<I>();
    }

    template <int I>
    size_t hash(auto* base)
    {
        return base->template hash<I>();
    }

    template <int I>
    void set_hash(auto* base, size_t hash)
    {
        base->template set_hash<I>(hash);
    }

    template <int I>
    void set_next_hashptr(auto* lhs, auto* rhs)
    {
        lhs->template set_next_hashptr<I>(rhs);
    }

} // namespace detail

} // namespace tmi

#endif // TMINODE_DETAIL_H
