//
// Copyright (c) 2007-2015 Andrey Semashev
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// This file contains a test boilerplate for checking that every public header
// is self-contained and does not have any missing #include-s.

#define BOOST_GIL_TEST_INCLUDE_HEADER() <boost/gil/BOOST_GIL_TEST_HEADER>

#include BOOST_GIL_TEST_INCLUDE_HEADER()

int main()
{
    return 0;
}
