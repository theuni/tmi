#include "tmi.h"

struct myclass {
    size_t val{0};
    myclass() = default;
    myclass(size_t rhs) : val(rhs) {}
    bool operator()(const myclass&) const { return true; }
    bool operator==(const myclass&) const = default;
};

struct comp_less;
struct comp_greater;
struct hash_unique;
struct hash_nonunique;

class compare_myclass_less
{
public:
    using sorted = std::true_type;
    static constexpr bool sorted_type() { return true;}
    using tag = comp_less;
    constexpr static bool sorted_unique() { return false; }
    constexpr bool operator()(const myclass& a, const myclass& b) const noexcept
    {
        return a.val < b.val;
    }
};

class compare_myclass_greater
{
public:
    using sorted = std::true_type;
    static constexpr bool sorted_type() { return true;}
    using tag = comp_greater;
    constexpr static bool sorted_unique() { return true; }
    bool operator()(const myclass& a, const myclass& b) const
    {
        return a.val > b.val;
    }
};

class hash_myclass_unique
{
public:
    using sorted = std::false_type;
    static constexpr bool sorted_type() { return false;}
    using tag = hash_unique;
    using hash_type = size_t;
    constexpr static bool hashed_unique() { return true; }
    bool operator()(const myclass& a, const myclass& b) const
    {
        return a == b;
    }
    size_t operator()(const hash_type& a) const
    {
        return a;
    }
    size_t operator()(const myclass& a) const
    {
        return a.val;
    }
    bool operator()(const myclass& a, const hash_type& b) const
    {
        return a.val == b;
    }
};

class hash_myclass
{
public:
    using sorted = std::false_type;
    static constexpr bool sorted_type() { return false;}
    using tag = hash_nonunique;
    using hash_type = size_t;
    constexpr static bool hashed_unique() { return false; }
    bool operator()(const myclass& a, const myclass& b) const
    {
        return a == b;
    }
    size_t operator()(const hash_type& a) const
    {
        return a;
    }
    size_t operator()(const myclass& a) const
    {
        return a.val;
    }
    bool operator()(const myclass& a, const hash_type& b) const
    {
        return a.val == b;
    }
};

template <typename T, typename... Indices>
struct index_helper {
    using value_type = T;
    using index_types = std::tuple<Indices...>;
};


using indices = index_helper<myclass, hash_myclass, hash_myclass_unique, compare_myclass_less, compare_myclass_greater>;

int main()
{
    tmi::tmi<myclass, indices, std::allocator<myclass>> bar;
    auto& hash_index = bar.get<hash_nonunique>();
    bar.emplace(0UL);
    for (unsigned i = 1; i <= 10; i++) {
        hash_index.emplace(i);
    }
    auto it = hash_index.find(0);
    hash_index.modify(it, [](myclass& rhs) {
        rhs.val = 11;
    });
    auto least_it = bar.get<comp_less>().begin();
    auto greatest_it = bar.get<comp_greater>().begin();
    assert(least_it->val == 1);
    assert(greatest_it->val == 11);
}
