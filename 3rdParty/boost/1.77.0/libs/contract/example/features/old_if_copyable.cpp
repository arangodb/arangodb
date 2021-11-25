
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <boost/type_traits.hpp>
#include <boost/noncopyable.hpp>
#include <cassert>

//[old_if_copyable_offset
template<typename T> // T might or might not be copyable.
void offset(T& x, int count) {
    // No compiler error if T has no copy constructor...
    boost::contract::old_ptr_if_copyable<T> old_x = BOOST_CONTRACT_OLDOF(x);
    boost::contract::check c = boost::contract::function()
        .postcondition([&] {
            // ...but old value null if T has no copy constructor.
            if(old_x) BOOST_CONTRACT_ASSERT(x == *old_x + count);
        })
    ;
    
    x += count;
}
//]

//[old_if_copyable_w_decl
// Copyable type but...
class w {
public:
    w(w const&) { /* Some very expensive copy operation here... */ }

    /* ... */
//]
    w() : num_(0) {}
    int operator+(int i) const { return num_ + i; }
    w& operator+=(int i) { num_ += i; return *this; }
    bool operator==(int i) const { return long(num_) == i; }
private:
    unsigned long num_;
};

//[old_if_copyable_w_spec
// ...never copy old values for type `w` (because its copy is too expensive).
namespace boost { namespace contract {
    template<>
    struct is_old_value_copyable<w> : boost::false_type {};
} }
//]

//[old_if_copyable_p_decl
// Non-copyable type but...
class p : private boost::noncopyable {
    int* num_;
    
    friend struct boost::contract::old_value_copy<p>;

    /* ... */
//]
public:
    p() : num_(new int(0)) {}
    ~p() { delete num_; }
    int operator+(int i) const { return *num_ + i; }
    p& operator+=(int i) { *num_ += i; return *this; }
    bool operator==(int i) const { return *num_ == i; }
};

//[old_if_copyable_p_spec
// ...still copy old values for type `p` (using a deep copy).
namespace boost { namespace contract {
    template<>
    struct old_value_copy<p> {
        explicit old_value_copy(p const& old) {
            *old_.num_ = *old.num_; // Deep copy pointed value.
        }

        p const& old() const { return old_; }

    private:
        p old_;
    };
    
    template<>
    struct is_old_value_copyable<p> : boost::true_type {};
} }
//]

//[old_if_copyable_n_decl
class n { // Do not want to use boost::noncopyable but...
    int num_;

private:
    n(n const&); // ...unimplemented private copy constructor (so non-copyable).

    /* ... */
//]

public:
    n() : num_(0) {}
    int operator+(int i) const { return num_ + i; }
    n& operator+=(int i) { num_ += i; return *this; }
    bool operator==(int i) const { return num_ == i; }
};

//[old_if_copyable_n_spec
// Specialize `boost::is_copy_constructible` (no need for this on C++11).
namespace boost { namespace contract {
    template<>
    struct is_old_value_copyable<n> : boost::false_type {};
} }
//]

int main() {
    int i = 0; // Copy constructor, copy and check old values.
    offset(i, 3);
    assert(i == 3);
    
    w j; // Expensive copy constructor, so never copy or check old values.
    offset(j, 3);
    assert(j == 3);

    p k; // No copy constructor, but still copy and check old values.
    offset(k, 3);
    assert(k == 3);

    n h; // No copy constructor, no compiler error but no old value checks.
    offset(h, 3);
    assert(h == 3);
    
    return 0;
}

