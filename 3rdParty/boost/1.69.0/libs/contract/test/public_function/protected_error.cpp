
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test a public function contract cannot override from a protected one.

#include <boost/contract/public_function.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/override.hpp>
#include <boost/contract/check.hpp>

struct b {
protected:
    virtual void f(boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::public_function(v, this);
    }
    
    friend class boost::contract::access; // Test this cannot prevent error.
};

struct a
    #define BASES public b
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    void f(boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<override_f>(
                v, &a::f, this); // Error (override cannot access b::f).
        #ifdef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
            #error "Forcing error even when public functions not checked"
        #endif
    }

    // Correctly, GCC and CLang cannot even see b::f as part of overloaded bases
    // because it is protected. MSVC also fails compilation but only when
    // override_f below tries to call b::f (because it is protected so it cannot
    // be seen from within override_f).
    BOOST_CONTRACT_OVERRIDE(f)
    
    friend class boost::contract::access; // Test this cannot prevent error.
};

int main() {
    a aa;
    aa.f();
    return 0;
}

