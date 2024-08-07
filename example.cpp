#include "tmi.h"

struct myclass {
    int val{0};
    myclass() = default;
    myclass(int rhs) : val(rhs) {}
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
    using tag = hash_unique;
    using hash_type = int;
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
    using tag = hash_nonunique;
    using hash_type = int;
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

template <typename T, typename... Comparators>
struct comparators_helper {
    using value_type = T;
    using comparator_types = std::tuple<Comparators...>;
};

template <typename T, typename... Hashers>
struct hashers_helper {
    using value_type = T;
    using hasher_types = std::tuple<Hashers...>;
};


using comparators = comparators_helper<myclass, compare_myclass_less, compare_myclass_greater>;
using hashers = hashers_helper<myclass, hash_myclass, hash_myclass_unique>;

int main()
{
    tmi::tmi<myclass, comparators, hashers, std::allocator<myclass>> bar;
    auto& hash_index = bar.get<hash_nonunique>();
    for (int i = 0; i <= 10; i++) {
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
