
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test all invariants (static, cv, and const-only).

#define BOOST_CONTRACT_TEST_STATIC_INV
#undef BOOST_CONTRACT_TEST_CV_INV
#undef BOOST_CONTRACT_TEST_CONST_INV
#include "decl.hpp"

