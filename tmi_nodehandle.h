// Copyright (c) 2024 Cory Fields
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TMI_NODEHANDLE_H_
#define TMI_NODEHANDLE_H_

#include "tmi_fwd.h"

namespace tmi {
namespace detail {

template <typename Allocator, typename Node>
class node_handle
{
    using node_type = Node;
    using allocator_type = Allocator;
    using node_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<node_type>;
    using value_type = typename Node::value_type;
    node_allocator_type m_alloc;
    node_type* m_node = nullptr;

    template <typename, typename, typename>
    friend class tmi::multi_index_container;

    constexpr node_handle(const node_allocator_type& alloc, node_type* node) noexcept : m_alloc(alloc), m_node(node){}

    void destroy()
    {
        if (m_node) {
            std::allocator_traits<node_allocator_type>::destroy(m_alloc, m_node);
            std::allocator_traits<node_allocator_type>::deallocate(m_alloc, m_node, 1);
            m_node = nullptr;
        }
    }

public:
    constexpr node_handle() noexcept {}
    node_handle(node_handle&& rhs) noexcept : m_alloc(std::move(rhs.m_alloc)), m_node(rhs.m_node)
    {
        rhs.m_node = nullptr;
    }
    node_handle& operator=(node_handle&& rhs)
    {
        bool was_empty = empty();
        destroy();
        m_node = rhs.m_node;
        rhs.m_node = nullptr;
        if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_move_assignment)
        {
            m_alloc = std::move(rhs.m_alloc);
        } else if (was_empty) {
            m_alloc = std::move(rhs.m_alloc);
        }
        return *this;
    }
    ~node_handle()
    {
        destroy();
    }
    bool empty() const noexcept
    {
        return m_node == nullptr;
    }
    explicit operator bool() const noexcept
    {
        return m_node != nullptr;
    }
    allocator_type get_allocator() const
    {
        return m_alloc;
    }
    value_type& value() const
    {
        return m_node->value();
    }
    void swap(node_handle& rhs) noexcept(std::allocator_traits<allocator_type>::propagate_on_container_swap::value ||
                                std::allocator_traits<allocator_type>::is_always_equal::value)
    {
        std::swap(m_node, rhs.m_node);
        if constexpr(std::allocator_traits<allocator_type>::propagate_on_container_swap)
        {
            if (!empty() || !rhs.empty()) {
                std::swap(m_alloc, rhs.m_alloc);
            }
        } else if (empty() != rhs.empty()) {
            std::swap(m_alloc, rhs.m_alloc);
        }
    }
};

} // namespace detail
} // namespace tmi

#endif // TMI_NODEHANDLE_H_
