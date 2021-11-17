
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <cassert>

//[static_public
template<class C>
class make {
public:
    static void static_invariant() { // Static class invariants.
        BOOST_CONTRACT_ASSERT(instances() >= 0);
    }

    // Contract for a static public function.
    static int instances() {
        // Explicit template parameter `make` (check static invariants).
        boost::contract::check c = boost::contract::public_function<make>();

        return instances_; // Function body.
    }

    /* ... */
//]

    make() : object() {
        boost::contract::old_ptr<int> old_instances =
                BOOST_CONTRACT_OLDOF(instances());
        boost::contract::check c = boost::contract::constructor(this)
            .postcondition([&] {
                BOOST_CONTRACT_ASSERT(instances() == *old_instances + 1);
            })
        ;

        ++instances_;
    }

    ~make() {
        boost::contract::old_ptr<int> old_instances =
                BOOST_CONTRACT_OLDOF(instances());
        boost::contract::check c = boost::contract::destructor(this)
            .postcondition([&] { // (An example of destructor postconditions.)
                BOOST_CONTRACT_ASSERT(instances() == *old_instances - 1);
            })
        ;

        --instances_;
    }
    
    C object;

private:
    static int instances_;
};

template<class C>
int make<C>::instances_ = 0;

int main() {
    struct x {};
    make<x> x1, x2, x3;
    assert(make<x>::instances() == 3);

    return 0;
}

