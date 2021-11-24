
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[mitchell02_decrement_button
#ifndef DECREMENT_BUTTON_HPP_
#define DECREMENT_BUTTON_HPP_

#include "push_button.hpp"
#include "counter.hpp"
#include "../observer/observer.hpp"
#include <boost/contract.hpp>
#include <boost/noncopyable.hpp>

class decrement_button
    #define BASES public push_button, public observer, \
            private boost::noncopyable
    : BASES
{
    friend class boost::contract::access;

    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES
    
    BOOST_CONTRACT_OVERRIDES(on_bn_clicked, up_to_date_with_subject, update);

public:
    /* Creation */

    explicit decrement_button(counter& a_counter) : counter_(a_counter) {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                // Enable iff positive value.
                BOOST_CONTRACT_ASSERT(enabled() == (a_counter.value() > 0));
            })
        ;
        counter_.attach(this);
    }

    // Destroy button.
    virtual ~decrement_button() {
        // Could have omitted contracts here (nothing to check).
        boost::contract::check c = boost::contract::destructor(this);
    }

    /* Commands */

    virtual void on_bn_clicked(boost::contract::virtual_* v = 0)
            /* override */ {
        boost::contract::old_ptr<int> old_value =
                BOOST_CONTRACT_OLDOF(v, counter_.value());
        boost::contract::check c = boost::contract::public_function<
            override_on_bn_clicked
        >(v, &decrement_button::on_bn_clicked, this)
            .postcondition([&] {
                // Counter decremented.
                BOOST_CONTRACT_ASSERT(counter_.value() == *old_value - 1);
            })
        ;
        counter_.decrement();
    }

    virtual bool up_to_date_with_subject(boost::contract::virtual_* v = 0)
            const /* override */ {
        bool result;
        boost::contract::check c = boost::contract::public_function<
            override_up_to_date_with_subject
        >(v, result, &decrement_button::up_to_date_with_subject, this);

        return result = true; // For simplicity, assume always up-to-date.
    }

    virtual void update(boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<
                override_update>(v, &decrement_button::update, this)
            .postcondition([&] {
                // Enabled iff positive value.
                BOOST_CONTRACT_ASSERT(enabled() == (counter_.value() > 0));
            })
        ;

        if(counter_.value() == 0) disable();
        else enable();
    }

private:
    counter& counter_;
};

#endif // #include guard
//]

