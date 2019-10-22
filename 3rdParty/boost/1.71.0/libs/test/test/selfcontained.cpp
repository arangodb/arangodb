// selfcontained.cpp
//
// Copyright (c) 2008
// Steven Watanabe
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_HEADER_TEST_STRINGIZE_IMPL(x) #x
#define BOOST_HEADER_TEST_STRINGIZE(x) BOOST_HEADER_TEST_STRINGIZE_IMPL(x)

#define BOOST_HEADER_TO_TEST BOOST_HEADER_TEST_STRINGIZE(BOOST_HEADER_TEST_NAME)

#include BOOST_HEADER_TO_TEST
