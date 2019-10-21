
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[mitchell02_observer_main
#include "observer/observer.hpp"
#include "observer/subject.hpp"
#include <boost/contract.hpp>
#include <cassert>

int test_state; // For testing only.

// Implement an actual subject.
class concrete_subject
    #define BASES public subject
    : BASES
{
    friend class boost::contract::access;

    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types; // Subcontracting.
    #undef BASES

public:
    typedef int state; // Some state being observed.

    concrete_subject() : state_() {
        // Could have omitted contracts here (nothing to check).
        boost::contract::check c = boost::contract::constructor(this);
    }

    virtual ~concrete_subject() {
        // Could have omitted contracts here (nothing to check).
        boost::contract::check c = boost::contract::destructor(this);
    }

    void set_state(state const& new_state) {
        // Could have omitted contracts here (nothing to check).
        boost::contract::check c = boost::contract::public_function(this);

        state_ = new_state;
        assert(state_ == test_state);
        notify(); // Notify all observers.
    }

    state get_state() const {
        // Could have omitted contracts here (nothing to check).
        boost::contract::check c = boost::contract::public_function(this);
        return state_;
    }

private:
    state state_;
};

// Implement an actual observer.
class concrete_observer
    #define BASES public observer
    : BASES
{
    friend class boost::contract::access;

    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types; // Subcontracting.
    #undef BASES

    BOOST_CONTRACT_OVERRIDES(up_to_date_with_subject, update)

public:
    // Create concrete observer.
    explicit concrete_observer(concrete_subject const& subj) :
            subject_(subj), observed_state_() {
        // Could have omitted contracts here (nothing to check).
        boost::contract::check c = boost::contract::constructor(this);
    }

    virtual ~concrete_observer() {
        // Could have omitted contracts here (nothing to check).
        boost::contract::check c = boost::contract::destructor(this);
    }

    // Implement base virtual functions.

    bool up_to_date_with_subject(boost::contract::virtual_* v = 0)
            const /* override */ {
        bool result;
        boost::contract::check c = boost::contract::public_function<
            override_up_to_date_with_subject
        >(v, result, &concrete_observer::up_to_date_with_subject, this);

        return result = true; // For simplicity, assume always up-to-date.
    }

    void update(boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<
                override_update>(v, &concrete_observer::update, this);

        observed_state_ = subject_.get_state();
        assert(observed_state_ == test_state);
    }

private:
    concrete_subject const& subject_;
    concrete_subject::state observed_state_;
};

int main() {
    concrete_subject subj;
    concrete_observer ob(subj);
    subj.attach(&ob);

    subj.set_state(test_state = 123);
    subj.set_state(test_state = 456);

    return 0;
}
//]

