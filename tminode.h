#ifndef TMINODE_H
#define TMINODE_H

#include "tminode_base.h"

namespace tmi {

template <typename T, int ComparatorSize, int HashSize>
class tminode
{
public:
    using base_type = tminode_base<T, ComparatorSize, HashSize>;
    T m_value;
    base_type m_base{};
    tminode* m_prev{nullptr};
    tminode* m_next{nullptr};


public:
    void reset()
    {
        m_base = {};
    }

    explicit tminode(tminode* prev, T elem) : m_prev(prev), m_value(std::move(elem))
    {
        if (m_prev) m_prev->m_next = this;
        m_base.m_node = this;
    }

    template <typename... Args>
    tminode(tminode* prev, Args&&... args) : m_value(std::forward<Args>(args)...), m_prev(prev)
    {
        if (m_prev) m_prev->m_next = this;
        m_base.m_node = this;
    }

    const T& value() const { return m_value; }
    T& value() { return m_value; }

    base_type* get_base()
    {
        return &m_base;
    }

    tminode* next() const { return m_next; }
    tminode* prev() const { return m_prev; }

    void unlink()
    {
        if (m_prev)
            m_prev->m_next = m_next;
        if (m_next)
            m_next->m_prev = m_prev;
    }
};

} // namespace tmi
#endif // TMINODE_H
