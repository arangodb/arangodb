
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[mitchell02_counter_main
#include "counter/counter.hpp"
#include "counter/decrement_button.hpp"
#include "observer/observer.hpp"
#include <cassert>

int test_counter;

class view_of_counter
    #define BASES public observer
    : BASES
{
    friend class boost::contract::access;

    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES
    
    BOOST_CONTRACT_OVERRIDES(up_to_date_with_subject, update)

public:
    /* Creation */

    // Create view associated with given counter.
    explicit view_of_counter(counter& a_counter) : counter_(a_counter) {
        // Could have omitted contracts here (nothing to check).
        boost::contract::check c = boost::contract::constructor(this);

        counter_.attach(this);
        assert(counter_.value() == test_counter);
    }

    // Destroy view.
    virtual ~view_of_counter() {
        // Could have omitted contracts here (nothing to check).
        boost::contract::check c = boost::contract::destructor(this);
    }

    /* Commands */

    virtual bool up_to_date_with_subject(boost::contract::virtual_* v = 0)
            const /* override */ {
        bool result;
        boost::contract::check c = boost::contract::public_function<
            override_up_to_date_with_subject
        >(v, result, &view_of_counter::up_to_date_with_subject, this);

        return result = true; // For simplicity, assume always up-to-date.
    }

    virtual void update(boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<
                override_update>(v, &view_of_counter::update, this);

        assert(counter_.value() == test_counter);
    }

private:
    counter& counter_;
};

int main() {
    counter cnt(test_counter = 1);
    view_of_counter view(cnt);

    decrement_button dec(cnt);
    assert(dec.enabled());

    test_counter--;
    dec.on_bn_clicked();
    assert(!dec.enabled());

    return 0;
}
//]

