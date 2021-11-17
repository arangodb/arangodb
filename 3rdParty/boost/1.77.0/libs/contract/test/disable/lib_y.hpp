
#ifndef BOOST_CONTRACT_TEST_LIB_Y_HPP_
#define BOOST_CONTRACT_TEST_LIB_Y_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include "lib_x.hpp"
#include <boost/contract.hpp> // All headers so test ODR for entire lib.
#include <boost/config.hpp>

#ifdef BOOST_CONTRACT_TEST_LIB_Y_DYN_LINK
    #ifdef BOOST_CONTRACT_TEST_LIB_Y_SOURCE
        #define BOOST_CONTRACT_TEST_LIB_Y_DECLSPEC BOOST_SYMBOL_EXPORT
    #else
        #define BOOST_CONTRACT_TEST_LIB_Y_DECLSPEC BOOST_SYMBOL_IMPORT
    #endif
#else
    #define BOOST_CONTRACT_TEST_LIB_Y_DECLSPEC /* nothing */
#endif

namespace lib_y_ { // Internal namepsace.
    void BOOST_CONTRACT_TEST_LIB_Y_DECLSPEC y_body();
}

inline void y() {
    using boost::contract::test::detail::out;
    boost::contract::check c = boost::contract::function()
        // Capturing [&] so out() visible in MSVC10 lambdas.
        .precondition([&] { out("y::pre\n"); })
        .old([&] { out("y::old\n"); })
        .postcondition([&] { out("y::post\n"); })
    ;
    lib_y_::y_body();
}

#endif

