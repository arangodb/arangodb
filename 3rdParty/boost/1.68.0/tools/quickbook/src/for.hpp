/*=============================================================================
    Copyright (c) 2017 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// Macro for C++11 range based for loop, with BOOST_FOREACH as a fallback.
// Can't use C++11 loop in Visual C++ 10/Visual Studio 2010 or gcc 4.4.
// BOOST_FOREACH was causing warnings in Visual C++ 14.11/Visual Studio 2017

#if !defined(BOOST_QUICKBOOK_FOR_HPP)
#define BOOST_QUICKBOOK_FOR_HPP

#include <boost/config.hpp>

#if !defined(BOOST_NO_CXX11_RANGE_BASED_FOR)
#define QUICKBOOK_FOR(x, y) for (x : y)
#else
#include <boost/foreach.hpp>
#define QUICKBOOK_FOR(x, y) BOOST_FOREACH (x, y)
#endif

#endif
