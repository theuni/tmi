#include "tmi.h"

struct myclass {
    int val{0};
    myclass() = default;
    myclass(int rhs) : val(rhs) {}
    bool operator()(const myclass&) const { return true; }
    bool operator==(const myclass&) const = default;
};

class compare_myclass_less
{
public:
    constexpr static bool sorted_unique() { return false; }
    constexpr bool operator()(const myclass& a, const myclass& b) const noexcept
    {
        return a.val < b.val;
    }
};

class compare_myclass_greater
{
public:
    constexpr static bool sorted_unique() { return true; }
    bool operator()(const myclass& a, const myclass& b) const
    {
        return a.val > b.val;
    }
};

class hash_myclass_unique
{
public:
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
    tmi<myclass, comparators, hashers, std::allocator<myclass>> bar;
    for (int i = 0; i <= 1000; i++) {
        bar.emplace(i);
    }
    auto it = bar.find<hash_myclass>(3);
    bar.modify(it, [](myclass& rhs) {
        rhs.val = 5;
    });
    it = bar.find<hash_myclass>(5);
    assert(it != bar.end());
}
