#include "tmi.h"
#include <string>

struct myclass {
    std::string val;
    myclass() = default;
    myclass(size_t rhs) : val(std::to_string(rhs)) {}
};

struct comp_less;
struct comp_greater;
struct hash_unique;
struct hash_nonunique;

class compare_myclass_less
{
public:
    constexpr bool operator()(const myclass& a, const myclass& b) const noexcept
    {
        return std::stol(a.val) < std::stol(b.val);
    }
};

class compare_myclass_greater
{
public:
    bool operator()(const myclass& a, const myclass& b) const
    {
        return std::stol(a.val) > std::stol(b.val);
    }
};

class myclass_key_from_value
{
public:
    using result_type = std::string;
    result_type operator()(const myclass& a) const
    {
        return a.val;
    }
};

class myclass_hash
{
public:
    size_t operator()(const myclass_key_from_value::result_type& a) const
    {
        return std::hash<std::string>{}(a);
    }
};

class myclass_pred
{
public:
    bool operator()(const myclass_key_from_value::result_type& a, const myclass_key_from_value::result_type& b) const
    {
        return a == b;
    }
};

int main()
{
    tmi::multi_index_container<myclass,tmi::indexed_by<tmi::hashed_unique<tmi::tag<hash_unique>, myclass_key_from_value, myclass_hash, myclass_pred>,
                     tmi::ordered_unique<tmi::tag<comp_less>, tmi::identity<myclass>, compare_myclass_less>,
                     tmi::ordered_unique<tmi::tag<comp_greater>, tmi::identity<myclass>, compare_myclass_greater>>,
                     std::allocator<myclass>> bar{std::make_tuple(std::make_tuple(32UL, myclass_key_from_value(), myclass_hash(), myclass_pred()), std::make_tuple(tmi::identity<myclass>(), compare_myclass_less()), std::make_tuple(tmi::identity<myclass>(), compare_myclass_greater()))};

    auto& hash_index = bar.get<hash_unique>();
    bar.emplace(0ul);
    for (unsigned i = 1; i <= 10; i++) {
        hash_index.emplace(i);
    }
    auto it = hash_index.find("0");
    hash_index.modify(it, [](myclass& rhs) {
        rhs.val = "11";
    });
    auto least_it = bar.get<comp_less>().begin();
    auto greatest_it = bar.get<comp_greater>().begin();
    assert(least_it->val == "1");
    assert(greatest_it->val == "11");
}
