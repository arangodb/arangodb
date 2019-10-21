
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test with default max arg number (leave MAX_ARGS #undef) and no variadic tpl.

#ifndef BOOST_NO_CXX11_VARIADIC_TEMPLATES
    #error "build must define BOOST_NO_CXX11_VARIADIC_TEMPLATES"
#endif
#include "max_args.hpp"

