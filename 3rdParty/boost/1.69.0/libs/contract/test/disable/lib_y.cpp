
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Force .cpp never check post/except.
#ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
    #error "build must define NO_POSTCONDITIONS"
#endif
#ifndef BOOST_CONTRACT_NO_EXCEPTS
    #error "build must define NO_EXCEPTS"
#endif

#define BOOST_CONTRACT_TEST_LIB_Y_SOURCE
#include "lib_y.hpp"

namespace lib_y_ {
    void y_body() {
        using boost::contract::test::detail::out;
        out("y::body\n");
    }
}

