
#ifndef BOOST_CONTRACT_TEST_LIB_A_HPP_
#define BOOST_CONTRACT_TEST_LIB_A_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include "../detail/counter.hpp"
#include <boost/contract.hpp> // All headers so test ODR for entire lib.
#include <boost/config.hpp>
#include <string>

#ifdef BOOST_CONTRACT_TEST_LIB_A_DYN_LINK
    #ifdef BOOST_CONTRACT_TEST_LIB_A_SOURCE
        #define BOOST_CONTRACT_TEST_LIB_A_DECLSPEC BOOST_SYMBOL_EXPORT
    #else
        #define BOOST_CONTRACT_TEST_LIB_A_DECLSPEC BOOST_SYMBOL_IMPORT
    #endif
#else
    #define BOOST_CONTRACT_TEST_LIB_A_DECLSPEC /* nothing */
#endif

#define BOOST_CONTRACT_TEST_DETAIL_OUT_DECLSPEC \
    BOOST_CONTRACT_TEST_LIB_A_DECLSPEC
#include "../detail/out.hpp"

struct BOOST_CONTRACT_TEST_LIB_A_DECLSPEC a {
    static void static_invariant();
    void invariant() const;

    struct x_tag;
    typedef boost::contract::test::detail::counter<x_tag, int> x_type;

    int f(x_type& x);

    static void disable_pre_failure();
    static void disable_post_failure();
    static void disable_entry_inv_failure();
    static void disable_exit_inv_failure();
    static void disable_inv_failure();
    static void disable_failure();
};

#ifdef BOOST_CONTRACT_HEADER_ONLY
    #include "lib_a_inlined.hpp"
#endif

#endif // #include guard

