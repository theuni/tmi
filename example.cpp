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
    TMI_CPP23_STATIC constexpr bool operator()(const myclass& a, const myclass& b) TMI_CONST_IF_NOT_CPP23_STATIC
    {
        return std::stol(a.val) < std::stol(b.val);
    }
    TMI_CPP23_STATIC constexpr bool operator()(const myclass* a, const myclass* b) TMI_CONST_IF_NOT_CPP23_STATIC
    {
        return std::stol(a->val) < std::stol(b->val);
    }
};

class compare_myclass_greater
{
public:
    TMI_CPP23_STATIC bool operator()(const myclass& a, const myclass& b) TMI_CONST_IF_NOT_CPP23_STATIC
    {
        return std::stol(a.val) > std::stol(b.val);
    }
    TMI_CPP23_STATIC bool operator()(const myclass* a, const myclass* b) TMI_CONST_IF_NOT_CPP23_STATIC
    {
        return std::stol(a->val) > std::stol(b->val);
    }
};

class myclass_key_from_value
{
public:
    using result_type = std::string;
    TMI_CPP23_STATIC constexpr const result_type& operator()(const myclass& a) TMI_CONST_IF_NOT_CPP23_STATIC noexcept
    {
        return a.val;
    }
    TMI_CPP23_STATIC constexpr const result_type& operator()(const myclass* a) TMI_CONST_IF_NOT_CPP23_STATIC noexcept
    {
        return a->val;
    }
};

class myclass_hash
{
public:
    TMI_CPP23_STATIC size_t operator()(const myclass_key_from_value::result_type& a) TMI_CONST_IF_NOT_CPP23_STATIC noexcept
    {
        return std::hash<std::string>{}(a);
    }
};

class myclass_pred
{
public:
    TMI_CPP23_STATIC constexpr bool operator()(const myclass_key_from_value::result_type& a, const myclass_key_from_value::result_type& b) TMI_CONST_IF_NOT_CPP23_STATIC noexcept
    {
        return a == b;
    }
};

int main()
{
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
        assert(hash_index.count("2"));
        auto it = hash_index.find("0");
        hash_index.modify(it, [](myclass& rhs) {
            rhs.val = "11";
        });
        auto least_it = bar.get<comp_less>().begin();
        auto nh = bar.get<comp_less>().extract(least_it);
        auto inserted = bar.get<comp_less>().insert(std::move(nh));
        auto greatest_it = bar.get<comp_greater>().begin();
        assert(least_it->val == "1");
        assert(greatest_it->val == "11");
    }
    // Pointer storage
    {
        tmi::multi_index_container<myclass*,tmi::indexed_by<tmi::hashed_unique<tmi::tag<hash_unique>, myclass_key_from_value, myclass_hash, myclass_pred>,
                         tmi::ordered_unique<tmi::tag<comp_less>, tmi::identity<myclass*>, compare_myclass_less>,
                         tmi::ordered_unique<tmi::tag<comp_greater>, tmi::identity<myclass*>, compare_myclass_greater>>,
                         std::allocator<myclass*>> bar{std::make_tuple(std::make_tuple(32UL, myclass_key_from_value(), myclass_hash(), myclass_pred()), std::make_tuple(tmi::identity<myclass*>(), compare_myclass_less()), std::make_tuple(tmi::identity<myclass*>(), compare_myclass_greater()))};
        auto& hash_index = bar.get<hash_unique>();
        std::array<myclass, 11> arr;
        arr[0] = 0;
        bar.emplace(&arr[0]);
        for (unsigned i = 1; i <= 10; i++) {
            arr[i] = i;
            hash_index.emplace(&arr[i]);
        }
        assert(hash_index.count("2"));
        auto it = hash_index.find("0");
        hash_index.modify(it, [](myclass* rhs) {
            rhs->val = "11";
        });
        auto least_it = bar.get<comp_less>().begin();
        auto nh = bar.get<comp_less>().extract(least_it);
        auto inserted = bar.get<comp_less>().insert(std::move(nh));
        auto greatest_it = bar.get<comp_greater>().begin();
        assert((*least_it)->val == "1");
        assert((*greatest_it)->val == "11");
        auto reverse_it = bar.iterator_to(&arr[0]);
        assert((*reverse_it)->val == "11");
    }
}
