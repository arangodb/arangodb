
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test override public function error on result type mismatch.

#include <boost/contract/public_function.hpp>
#include <boost/contract/override.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/check.hpp>

struct b {
    virtual int f(boost::contract::virtual_* v = 0) {
        // Unfortunately, this cannot be made to error at compile-time because
        // in this case public_function does not that &b::f as param (but this
        // will error at run-time on a virtual call via a derived class).
        char result;
        boost::contract::check c = boost::contract::public_function(
                v, result, this);
        return result;
    }
};

struct a
    #define BASES public b
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    virtual int f(boost::contract::virtual_* v = 0) /* override */ {
        char result;
        boost::contract::check c = boost::contract::public_function<override_f>(
                v, result, &a::f, this); // Error (result time mismatch).
        #ifdef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
            #error "Forcing error even when public functions not checked"
        #endif
        return result;
    }
    BOOST_CONTRACT_OVERRIDE(f)
};

int main() {
    a aa;
    aa.f();
    return 0;
}

