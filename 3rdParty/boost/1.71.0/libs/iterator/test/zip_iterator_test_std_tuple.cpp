// Copyright (c) 2014 Kohei Takahashi.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
//  See http://www.boost.org for most recent version including documentation.

#include <boost/config.hpp>

#if !defined(BOOST_NO_CXX11_HDR_TUPLE) && !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

#include <tuple>
#include <boost/fusion/adapted/std_tuple.hpp>

#define TUPLE      std::tuple
#define MAKE_TUPLE std::make_tuple

#include "detail/zip_iterator_test.ipp"

#else

int main()
{
    return 0;
}

#endif

