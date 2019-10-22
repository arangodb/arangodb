
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[mitchell02_counter
#ifndef COUNTER_HPP_
#define COUNTER_HPP_

#include "../observer/subject.hpp"
#include <boost/contract.hpp>

class counter
    #define BASES public subject
    : BASES
{
    friend class boost::contract::access;

    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

public:
    /* Creation */

    // Construct counter with specified value.
    explicit counter(int a_value = 10) : value_(a_value) {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(value() == a_value); // Value set.
            })
        ;
    }

    // Destroy counter.
    virtual ~counter() {
        // Could have omitted contracts here (nothing to check).
        boost::contract::check c = boost::contract::destructor(this);
    }

    /* Queries */

    // Current counter value.
    int value() const {
        // Could have omitted contracts here (nothing to check).
        boost::contract::check c = boost::contract::public_function(this);
        return value_;
    }

    /* Commands */

    // Decrement counter value.
    void decrement() {
        boost::contract::old_ptr<int> old_value = BOOST_CONTRACT_OLDOF(value());
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(value() == *old_value - 1); // Decrement.
            })
        ;

        --value_;
        notify(); // Notify all attached observers.
    }

private:
    int value_;
};

#endif // #include guard
//]

