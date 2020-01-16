
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <cassert>

//[volatile
class u {
public:
    static void static_invariant();     // Static invariants.
    void invariant() const volatile;    // Volatile invariants.
    void invariant() const;             // Const invariants.

    u() { // Check static, volatile, and const invariants.
        boost::contract::check c= boost::contract::constructor(this);
    }

    ~u() { // Check static, volatile, and const invariants.
        boost::contract::check c = boost::contract::destructor(this);
    }

    void nc() { // Check static and const invariants.
        boost::contract::check c = boost::contract::public_function(this);
    }

    void c() const { // Check static and const invariants.
        boost::contract::check c = boost::contract::public_function(this);
    }
    
    void v() volatile { // Check static and volatile invariants.
        boost::contract::check c = boost::contract::public_function(this);
    }
    
    void cv() const volatile { // Check static and volatile invariants.
        boost::contract::check c = boost::contract::public_function(this);
    }

    static void s() { // Check static invariants only.
        boost::contract::check c = boost::contract::public_function<u>();
    }
};
//]

bool static_inv_checked, cv_inv_checked, const_inv_checked;
void u::static_invariant() { static_inv_checked = true; }
void u::invariant() const volatile { cv_inv_checked = true; }
void u::invariant() const { const_inv_checked = true; }

int main() {
    #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
        {
            static_inv_checked = cv_inv_checked = const_inv_checked = false;
            u x;
            assert(static_inv_checked);
            assert(cv_inv_checked);
            assert(const_inv_checked);

            static_inv_checked = cv_inv_checked = const_inv_checked = false;
            x.nc();
            assert(static_inv_checked);
            assert(!cv_inv_checked);
            assert(const_inv_checked);
            
            static_inv_checked = cv_inv_checked = const_inv_checked = false;
            x.c();
            assert(static_inv_checked);
            assert(!cv_inv_checked);
            assert(const_inv_checked);
            
            static_inv_checked = cv_inv_checked = const_inv_checked = false;
            x.v();
            assert(static_inv_checked);
            assert(cv_inv_checked);
            assert(!const_inv_checked);
            
            static_inv_checked = cv_inv_checked = const_inv_checked = false;
            x.cv();
            assert(static_inv_checked);
            assert(cv_inv_checked);
            assert(!const_inv_checked);
            
            static_inv_checked = cv_inv_checked = const_inv_checked = false;
            x.s();
            assert(static_inv_checked);
            assert(!cv_inv_checked);
            assert(!const_inv_checked);
        
            static_inv_checked = cv_inv_checked = const_inv_checked = false;
        } // Call destructor.
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            assert(static_inv_checked);
            assert(cv_inv_checked);
            assert(const_inv_checked);
        #endif
    #endif

    return 0;
}

