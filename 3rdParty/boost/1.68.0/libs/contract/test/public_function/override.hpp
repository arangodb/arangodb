
// no #include guard

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test error if override func does not actually override (unless PERMISSIVE).

#include <boost/contract/public_function.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/override.hpp>
#include <boost/contract/check.hpp>

struct b {
    virtual void g(boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::public_function(v, this);
    }
};

struct a
    #define BASES public b
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    virtual void f(boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<
                override_f>(v, &a::f, this);
    }
    BOOST_CONTRACT_OVERRIDE(f)
};

int main() {
    a aa;
    aa.f();
    return 0;
}

