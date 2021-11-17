
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <limits>
#include <cassert>

//[private_protected_virtual_counter
class counter {
    // Virtual private and protected functions still declare extra
    // `virtual_* = 0` parameter (otherwise they cannot be overridden), but...
protected:
    virtual void set(int n, boost::contract::virtual_* = 0) {
        boost::contract::check c = boost::contract::function() // ...no `v`.
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(n <= 0);
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(get() == n);
            })
        ;

        n_ = n;
    }

private:
    virtual void dec(boost::contract::virtual_* = 0) {
        boost::contract::old_ptr<int> old_get = BOOST_CONTRACT_OLDOF(get()); // ...no `v`.
        boost::contract::check c = boost::contract::function() // ...no `v`.
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(
                        get() + 1 >= std::numeric_limits<int>::min());
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(get() == *old_get - 1);
            })
        ;

        set(get() - 1);
    }
    
    int n_;

    /* ... */
//]

public:
    virtual int get(boost::contract::virtual_* v = 0) const {
        int result;
        boost::contract::check c = boost::contract::public_function(
                v, result, this)
            .postcondition([&] (int const result) {
                BOOST_CONTRACT_ASSERT(result <= 0);
                BOOST_CONTRACT_ASSERT(result == n_);
            })
        ;

        return result = n_;
    }

    counter() : n_(0) {} // Should contract constructor and destructor too.
    
    void invariant() const {
        BOOST_CONTRACT_ASSERT(get() <= 0);
    }

    friend int main();
};

//[private_protected_virtual_counter10
class counter10 
    #define BASES public counter
    : BASES
{
public:
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES
    
    // Overriding from non-public members so no subcontracting, no override_...
    
    virtual void set(int n, boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(n % 10 == 0);
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(get() == n);
            })
        ;

        counter::set(n);
    }

    virtual void dec(boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::old_ptr<int> old_get = BOOST_CONTRACT_OLDOF(v, get());
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([&] {
                BOOST_CONTRACT_ASSERT(
                        get() + 10 >= std::numeric_limits<int>::min());
            })
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(get() == *old_get - 10);
            })
        ;

        set(get() - 10);
    }
    
    /* ... */
//]
    
    virtual int get(boost::contract::virtual_* v = 0) const {
        int result;
        boost::contract::check c = boost::contract::public_function<
                override_get>(v, result, &counter10::get, this);

        return result = counter::get();
    }
    BOOST_CONTRACT_OVERRIDE(get)

    // Should contract default constructor and destructor too.

    void invariant() const {
        BOOST_CONTRACT_ASSERT(get() % 10 == 0);
    }
};

int main() {
    counter cnt;
    assert(cnt.get() == 0);
    cnt.dec();
    assert(cnt.get() == -1);

    counter10 cnt10;
    counter& b = cnt10; // Polymorphic calls.
    assert(b.get() == 0);
    b.dec();
    assert(b.get() == -10);

    return 0;
}

