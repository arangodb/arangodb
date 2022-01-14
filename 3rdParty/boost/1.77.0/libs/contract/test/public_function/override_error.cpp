
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test error if override func does not actually override.

#undef BOOST_CONTRACT_PERMISSIVE
#include "override.hpp"
#ifdef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
    #error "Forcing error even when public functions not checked"
#endif

