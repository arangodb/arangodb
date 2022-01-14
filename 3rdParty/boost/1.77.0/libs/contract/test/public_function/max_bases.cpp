
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test with max possible number of bases.

#include <boost/contract/public_function.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/check.hpp>
#include <boost/contract/override.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/repetition/enum.hpp>
#include <boost/preprocessor/cat.hpp>

// Limited by max size of current impl of Boost.MPL vector.
#ifndef BOOST_CONTRACT_TEST_CONFIG_MAX_BASES
    #define BOOST_CONTRACT_TEST_CONFIG_MAX_BASES 20
#endif

#define BOOST_CONTRACT_TEST_base_decl(z, n, unused) \
    struct BOOST_PP_CAT(b, n) { \
        virtual void f(boost::contract::virtual_* v = 0) { \
            boost::contract::check c = boost::contract::public_function( \
                    v, this); \
        } \
    };

BOOST_PP_REPEAT(BOOST_CONTRACT_TEST_CONFIG_MAX_BASES,
        BOOST_CONTRACT_TEST_base_decl, ~)

#define BOOST_CONTRACT_TEST_public_base(z, n, unused) public BOOST_PP_CAT(b, n)

struct a
    #define BASES \
        BOOST_PP_ENUM(BOOST_CONTRACT_TEST_CONFIG_MAX_BASES, \
                BOOST_CONTRACT_TEST_public_base, ~)
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    void f(boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<override_f>(
                v, &a::f, this);
    }
    BOOST_CONTRACT_OVERRIDE(f)
};

int main() {
    a aa;
    aa.f();
    return 0;
}

#undef BOOST_CONTRACT_TEST_base_decl
#undef BOOST_CONTRACT_TEST_public_base

