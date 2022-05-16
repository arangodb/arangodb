
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <boost/config.hpp>
#include <cassert>
    
#ifdef BOOST_GCC // G++ does not support static union members yet.
    int instances_ = 0;
#endif

//[union
union positive {
public:
    static void static_invariant() { // Static class invariants (as usual).
        BOOST_CONTRACT_ASSERT(instances() >= 0);
    }
    
    void invariant() const { // Class invariants (as usual).
        BOOST_CONTRACT_ASSERT(i_ > 0);
        BOOST_CONTRACT_ASSERT(d_ > 0);
    }

    // Contracts for constructor, as usual but...
    explicit positive(int x) : d_(0) {
        // ...unions cannot have bases so constructor preconditions here.
        boost::contract::constructor_precondition<positive> pre([&] {
            BOOST_CONTRACT_ASSERT(x > 0);
        });
        boost::contract::old_ptr<int> old_instances =
                BOOST_CONTRACT_OLDOF(instances());
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                { int y; get(y); BOOST_CONTRACT_ASSERT(y == x); }
                BOOST_CONTRACT_ASSERT(instances() == *old_instances + 1);
            })
        ;

        i_ = x;
        ++instances_;
    }
    
    // Contracts for destructor (as usual).
    ~positive() {
        boost::contract::old_ptr<int> old_instances =
                BOOST_CONTRACT_OLDOF(instances());
        boost::contract::check c = boost::contract::destructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(instances() == *old_instances - 1);
            })
        ;

        --instances_;
    }
    
    // Contracts for public function (as usual, but no virtual or override).
    void get(int& x) const {
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(x > 0);
            })
        ;
        
        x = i_;
    }

    // Contracts for static public function (as usual).
    static int instances() {
        boost::contract::check c = boost::contract::public_function<positive>();
        return instances_;
    }

private:
    int i_;
    double d_;
    
    /* ... */
//]

public:
    explicit positive(double x) : d_(0) {
        // Unions cannot have bases so constructor preconditions here.
        boost::contract::constructor_precondition<positive> pre([&] {
            BOOST_CONTRACT_ASSERT(x > 0);
        });
        boost::contract::old_ptr<int> old_instances =
                BOOST_CONTRACT_OLDOF(instances());
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                { double y; get(y); BOOST_CONTRACT_ASSERT(y == x); }
                BOOST_CONTRACT_ASSERT(instances() == *old_instances + 1);
            })
        ;

        d_ = x;
        ++instances_;
    }
    
    void get(double& x) const {
        boost::contract::check c = boost::contract::public_function(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(x > 0);
            })
        ;
        
        x = d_;
    }

    #ifndef BOOST_GCC // G++ does not support static union members yet.
        static int instances_;
    #endif
};

#ifndef BOOST_GCC // G++ does not support static union members yet.
    int positive::instances_ = 0;
#endif

int main() {
    {
        positive p(123);
        assert(p.instances() == 1);
        { int y = -456; p.get(y); assert(y == 123); }

        positive q(1.23);
        assert(q.instances() == 2);
        { double y = -4.56; q.get(y); assert(y == 1.23); }
    }
    assert(positive::instances() == 0);
    return 0;
}

