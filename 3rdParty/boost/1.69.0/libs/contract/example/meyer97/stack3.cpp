
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[meyer97_stack3
// File: stack3.cpp
#include "stack4.hpp"
#include <boost/contract.hpp>
#include <boost/optional.hpp>
#include <cassert>

// Dispenser LIFO with max capacity using error codes.
template<typename T>
class stack3 {
    friend class boost::contract::access;

    void invariant() const {
        if(!error()) {
            BOOST_CONTRACT_ASSERT(count() >= 0); // Count non-negative.
            BOOST_CONTRACT_ASSERT(count() <= capacity()); // Count bounded.
            // Empty if no element.
            BOOST_CONTRACT_ASSERT(empty() == (count() == 0));
        }
    }

public:
    enum error_code {
        no_error = 0,
        overflow_error,
        underflow_error,
        size_error
    };

    /* Initialization */

    // Create stack for max of n elems, if n < 0 set error (no preconditions).
    explicit stack3(int n, T const& default_value = T()) :
            stack_(0), error_(no_error) {
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                // Error if impossible.
                BOOST_CONTRACT_ASSERT((n < 0) == (error() == size_error));
                // No error if possible.
                BOOST_CONTRACT_ASSERT((n >= 0) == !error());
                // Created if no error.
                if(!error()) BOOST_CONTRACT_ASSERT(capacity() == n);
            })
        ;

        if(n >= 0) stack_ = stack4<T>(n);
        else error_ = size_error;
    }

    /* Access */

    // Max number of stack elements.
    int capacity() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return stack_.capacity();
    }

    // Number of stack elements.
    int count() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return stack_.count();
    }

    // Top element if present, otherwise none and set error (no preconditions).
    boost::optional<T const&> item() const {
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                // Error if impossible.
                BOOST_CONTRACT_ASSERT(empty() == (error() == underflow_error));
                // No error if possible.
                BOOST_CONTRACT_ASSERT(!empty() == !error());
            })
        ;

        if(!empty()) {
            error_ = no_error;
            return boost::optional<T const&>(stack_.item());
        } else {
            error_ = underflow_error;
            return boost::optional<T const&>();
        }
    }

    /* Status Report */

    // Error indicator set by various operations.
    error_code error() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return error_;
    }

    bool empty() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return stack_.empty();
    }

    bool full() const {
        // Check invariants.
        boost::contract::check c = boost::contract::public_function(this);
        return stack_.full();
    }

    /* Element Change */

    // Add x to top if capacity allows, otherwise set error (no preconditions).
    void put(T const& x) {
        boost::contract::old_ptr<bool> old_full = BOOST_CONTRACT_OLDOF(full());
        boost::contract::old_ptr<int> old_count = BOOST_CONTRACT_OLDOF(count());
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                // Error if impossible.
                BOOST_CONTRACT_ASSERT(*old_full == (error() == overflow_error));
                // No error if possible.
                BOOST_CONTRACT_ASSERT(!*old_full == !error());
                if(!error()) { // If no error...
                    BOOST_CONTRACT_ASSERT(!empty()); // ...not empty.
                    BOOST_CONTRACT_ASSERT(*item() == x); // ...added to top.
                    // ...one more.
                    BOOST_CONTRACT_ASSERT(count() == *old_count + 1);
                }
            })
        ;

        if(full()) error_ = overflow_error;
        else {
            stack_.put(x);
            error_ = no_error;
        }
    }

    // Remove top element if possible, otherwise set error (no preconditions).
    void remove() {
        boost::contract::old_ptr<bool> old_empty =
                BOOST_CONTRACT_OLDOF(empty());
        boost::contract::old_ptr<int> old_count = BOOST_CONTRACT_OLDOF(count());
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                // Error if impossible.
                BOOST_CONTRACT_ASSERT(*old_empty == (error() ==
                        underflow_error));
                // No error if possible.
                BOOST_CONTRACT_ASSERT(!*old_empty == !error());
                if(!error()) { // If no error...
                    BOOST_CONTRACT_ASSERT(!full()); // ...not full.
                    // ...one less.
                    BOOST_CONTRACT_ASSERT(count() == *old_count - 1);
                }
            })
        ;

        if(empty()) error_ = underflow_error;
        else {
            stack_.remove();
            error_ = no_error;
        }
    }

private:
    stack4<T> stack_;
    mutable error_code error_;
};

int main() {
    stack3<int> s(3);
    assert(s.capacity() == 3);
    assert(s.count() == 0);
    assert(s.empty());
    assert(!s.full());

    s.put(123);
    assert(!s.empty());
    assert(!s.full());
    assert(*s.item() == 123);
    
    s.remove();
    assert(s.empty());
    assert(!s.full());

    return 0;
}
//]

