/*=============================================================================
    Copyright (c) 2015 Kohei Takahashi

    Use modification and distribution are subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
==============================================================================*/

#include <boost/config.hpp>

#if defined(BOOST_NO_CXX11_HDR_TUPLE) || \
    defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#   error "does not meet requirements"
#endif

#include <tuple>
#include <boost/fusion/include/std_tuple.hpp>

#define FUSION_SEQUENCE std::tuple
#include "convert.hpp"
