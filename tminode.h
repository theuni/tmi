#ifndef TMINODE_H
#define TMINODE_H

#include <array>
#include <cstdint>
#include <limits>
#include <cstdlib>

namespace tmi {

template <typename T, int ComparatorSize, int NodeSize>
class tminode;

template <typename T, int ComparatorSize, int NodeSize>
struct tminode_base {
        struct rb {
            tminode_base* m_left{nullptr};
            tminode_base* m_right{nullptr};
            tminode_base* m_parent{nullptr};
        };
        struct hash {
            tminode_base* m_nexthash{nullptr};
            size_t m_hash{0};
        };

        /* Pointer back to self. This is a hack which enables the tminode_base
           structure to be instantiated as required by the red-black-tree
           implementation. See note towards the top of manysortfind.h for
           more tminode_base.

           libc++ uses inheritance to create a base class which contains only
           pointer/color tminode_base. That doesn't work here as we require T to be the
           first member to enable casting T* to node*. See note in
           manysortfind::iterator_to.

           Ideally this will be removed once the libc++ red-black-tree
           functions have been replaced.

        */
        tminode<T, ComparatorSize, NodeSize>* m_node{nullptr};

        std::array<rb, ComparatorSize> m_tree_pointers{};
        std::array<hash, NodeSize> m_hash_pointers{};

    public:
        friend class tminode<T, ComparatorSize, NodeSize>;
        enum Color : bool {
            RED = false,
            BLACK = true
        };

        template <int I>
        void set_right(tminode_base* rhs)
        {
            std::get<I>(m_tree_pointers).m_right = rhs;
        }

        template <int I>
        void set_left(tminode_base* rhs)
        {
            std::get<I>(m_tree_pointers).m_left = rhs;
        }


        /* The following functions use Boost's pointer compression trick to
           encode the red/black bit in the parent pointer. It makes the
           assumption that no sane compiler will ever allow this pointer to
           be set to an odd memory address. */

        template <int I>
        tminode_base* parent() const
        {
            static constexpr uintptr_t mask = std::numeric_limits<uintptr_t>::max() - 1;
            auto addr = reinterpret_cast<uintptr_t>(std::get<I>(m_tree_pointers).m_parent) & mask;
            return reinterpret_cast<tminode_base*>(addr);
        }

        template <int I>
        void set_parent(tminode_base* rhs)
        {
            static constexpr uintptr_t mask = 1;
            auto prev = reinterpret_cast<uintptr_t>(std::get<I>(m_tree_pointers).m_parent) & mask;
            auto newaddr = reinterpret_cast<uintptr_t>(rhs) | prev;
            std::get<I>(m_tree_pointers).m_parent = reinterpret_cast<tminode_base*>(newaddr);
        }

        template <int I>
        Color color() const
        {
            static constexpr uintptr_t mask = 1;
            return (reinterpret_cast<uintptr_t>(std::get<I>(m_tree_pointers).m_parent) & mask) == 0 ? Color::RED : Color::BLACK;
        }

        template <int I>
        void set_color(Color rhs)
        {
            static constexpr uintptr_t mask = std::numeric_limits<uintptr_t>::max() - 1;
            auto addr = reinterpret_cast<uintptr_t>(std::get<I>(m_tree_pointers).m_parent) & mask;
            std::get<I>(m_tree_pointers).m_parent = reinterpret_cast<tminode_base*>(addr | static_cast<uintptr_t>(rhs));
        }

        template <int I>
        tminode_base* left() const
        {
            return std::get<I>(m_tree_pointers).m_left;
        }

        template <int I>
        tminode_base* right() const
        {
            return std::get<I>(m_tree_pointers).m_right;
        }


        tminode<T, ComparatorSize, NodeSize>* node() const
        {
            return m_node;
        }

        template <int I>
        tminode_base* next_hash() const
        {
            return std::get<I>(m_hash_pointers).m_nexthash;
        }

        template <int I>
        size_t hash()
        {
            return std::get<I>(m_hash_pointers).m_hash;
        }

        template <int I>
        void set_hash(size_t hash)
        {
            std::get<I>(m_hash_pointers).m_hash = hash;
        }

        template <int I>
        void set_next_hashptr(tminode_base* rhs)
        {
            std::get<I>(m_hash_pointers).m_nexthash = rhs;
        }
    };

template <typename T, int ComparatorSize, int NodeSize>
class tminode
{
public:
    using base_type = tminode_base<T, ComparatorSize, NodeSize>;
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
