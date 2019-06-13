
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test pre disable no assertion (in programs, but same for libraries).

#ifndef BOOST_CONTRACT_PRECONDITIONS_DISABLE_NO_ASSERTION
    #error "build must define PRECONDITIONS_DISABLE_NO_ASSERTION"
#endif
#include "prog.hpp"

