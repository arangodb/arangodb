
#ifndef BOOST_CONTRACT_TEST_LIB_B_HPP_
#define BOOST_CONTRACT_TEST_LIB_B_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/config.hpp>

#ifdef BOOST_CONTRACT_TEST_LIB_B_DYN_LINK
    #ifdef BOOST_CONTRACT_TEST_LIB_B_SOURCE
        #define BOOST_CONTRACT_TEST_LIB_B_DECLSPEC BOOST_SYMBOL_EXPORT
    #else
        #define BOOST_CONTRACT_TEST_LIB_B_DECLSPEC BOOST_SYMBOL_IMPORT
    #endif
#else
    #define BOOST_CONTRACT_TEST_LIB_B_DECLSPEC /* nothing */
#endif

bool BOOST_CONTRACT_TEST_LIB_B_DECLSPEC call_f();

struct BOOST_CONTRACT_TEST_LIB_B_DECLSPEC b {
    static void static_invariant();
    void invariant() const;

    void g();

    static bool test_disable_pre_failure();
    static bool test_disable_post_failure();
    static bool test_disable_entry_inv_failure();
    static bool test_disable_exit_inv_failure();
    static bool test_disable_inv_failure();
    static bool test_disable_failure();
    
};

#endif // #include guard


