#ifndef TMI_NODE_H
#define TMI_NODE_H

#include "tmi_index.h"

#include <array>
#include <cstdint>
#include <limits>
#include <cstdlib>
#include <tuple>
#include <type_traits>
#include <utility>

namespace tmi {

template <typename T, typename Indices>
class tminode {
    struct rb {
        tminode* m_left{nullptr};
        tminode* m_right{nullptr};
        tminode* m_parent{nullptr};
        int m_bf{0};
    };
    struct hash {
        tminode* m_nexthash{nullptr};
        size_t m_hash{0};
    };

    using index_types = typename Indices::index_types;
    template <int I>
    struct base_index_type_helper
    {
        using index_type = std::tuple_element_t<I, index_types>;
        using data_type = std::conditional_t<std::is_base_of_v<detail::hashed_type, index_type>, hash, rb>;
    };

    template <typename>
    struct base_index_helper;
    template <size_t... ints>
    struct base_index_helper<std::index_sequence<ints...>> {
        using data_types = std::tuple<typename base_index_type_helper<ints>::data_type ...>;
    };
    static constexpr size_t num_indices = std::tuple_size<index_types>();
    using data_types_tuple = typename base_index_helper<std::make_index_sequence<num_indices>>::data_types;

    T m_value;
    data_types_tuple m_data;
    tminode* m_prev{nullptr};
    tminode* m_next{nullptr};

public:
    tminode(const tminode& rhs) = default;

    explicit tminode(const T& elem) : m_value(elem)
    {
    }

    template <typename... Args>
    tminode(std::in_place_t, Args&&... args) : m_value(std::forward<Args>(args)...)
    {
    }

    const T& value() const { return m_value; }
    T& value() { return m_value; }

    tminode* next() const { return m_next; }
    tminode* prev() const { return m_prev; }

    void link(tminode* prev)
    {
        if (prev) {
            prev->m_next = this;
        }
        m_prev = prev;
    }
    void unlink()
    {
        if (m_prev)
            m_prev->m_next = m_next;
        if (m_next)
            m_next->m_prev = m_prev;
    }

    template <int I>
    void set_right(tminode* rhs)
    {
        std::get<I>(m_data).m_right = rhs;
    }

    template <int I>
    void set_left(tminode* rhs)
    {
        std::get<I>(m_data).m_left = rhs;
    }

    template <int I>
    tminode* parent() const
    {
        return std::get<I>(m_data).m_parent;
    }

    template <int I>
    void set_parent(tminode* rhs)
    {
        std::get<I>(m_data).m_parent = rhs;
    }


    template <int I>
    void set_bf(int bf)
    {
        std::get<I>(m_data).m_bf = bf;
    }

    template <int I>
    int bf() const
    {
        std::get<I>(m_data).m_bf;
    }

    template <int I>
    tminode* left() const
    {
        return std::get<I>(m_data).m_left;
    }

    template <int I>
    tminode* right() const
    {
        return std::get<I>(m_data).m_right;
    }


    template <int I>
    tminode* next_hash() const
    {
        return std::get<I>(m_data).m_nexthash;
    }

    template <int I>
    size_t hash() const
    {
        return std::get<I>(m_data).m_hash;
    }

    template <int I>
    void set_hash(size_t hash)
    {
        std::get<I>(m_data).m_hash = hash;
    }

    template <int I>
    void set_next_hashptr(tminode* rhs)
    {
        std::get<I>(m_data).m_nexthash = rhs;
    }
};

} // namespace tmi
#endif // TMI_NODE_H
