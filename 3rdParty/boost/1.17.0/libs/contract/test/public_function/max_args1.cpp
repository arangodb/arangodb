
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test with max argument number set to 1.

#if !defined(BOOST_CONTRACT_MAX_ARGS) || BOOST_CONTRACT_MAX_ARGS != 1
    #error "build must define MAX_ARGS=1"
#endif
#include "max_args.hpp"

