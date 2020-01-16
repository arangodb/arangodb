/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License,
Version 1.0. (See accompanying file LICENSE_1_0.txt
or copy at http://www.boost.org/LICENSE_1_0.txt)
*/

#ifdef TEST_STD
#include <type_traits>
#else
#include <boost/type_traits/copy_reference.hpp>
#endif
#include "test.hpp"
#include "check_type.hpp"

TT_TEST_BEGIN(copy_reference)

BOOST_CHECK_TYPE3(::tt::copy_reference<int, char>::type, int);
BOOST_CHECK_TYPE3(::tt::copy_reference<int, char&>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_reference<int&, char>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_reference<int&, char&>::type, int&);

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
BOOST_CHECK_TYPE3(::tt::copy_reference<int, char&&>::type, int&&);
BOOST_CHECK_TYPE3(::tt::copy_reference<int&, char&&>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_reference<int&&, char>::type, int&&);
BOOST_CHECK_TYPE3(::tt::copy_reference<int&&, char&>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_reference<int&&, char&&>::type, int&&);
#endif

TT_TEST_END
