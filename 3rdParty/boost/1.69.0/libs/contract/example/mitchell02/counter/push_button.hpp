
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[mitchell02_push_button
#ifndef PUSH_BUTTON_HPP_
#define PUSH_BUTTON_HPP_

#include <boost/contract.hpp>
#include <cassert>

class push_button {
public:
    // No inv and no bases so contracts optional if no pre, post, and override.

    /* Creation */

    // Create an enabled button.
    push_button() : enabled_(true) {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(enabled()); // Enabled.
            })
        ;
    }

    // Destroy button.
    virtual ~push_button() {
        // Could have omitted contracts here (nothing to check).
        boost::contract::check c = boost::contract::destructor(this);
    }

    /* Queries */

    // If button is enabled.
    bool enabled() const {
        // Could have omitted contracts here (nothing to check).
        boost::contract::check c = boost::contract::public_function(this);
        return enabled_;
    }

    /* Commands */

    // Enable button.
    void enable() {
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(enabled()); // Enabled.
            })
        ;

        enabled_ = true;
    }

    // Disable button.
    void disable() {
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(!enabled()); // Disabled.
            })
        ;

        enabled_ = false;
    }

    // Invoke externally when button clicked.
    virtual void on_bn_clicked(boost::contract::virtual_* v = 0) = 0;

private:
    bool enabled_;
};

void push_button::on_bn_clicked(boost::contract::virtual_* v) {
    boost::contract::check c = boost::contract::public_function(v, this)
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(enabled()); // Enabled.
        })
    ;
    assert(false);
}

#endif // #include guard
//]

