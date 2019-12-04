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
#include <boost/type_traits/copy_cv_ref.hpp>
#endif
#include "test.hpp"
#include "check_type.hpp"

TT_TEST_BEGIN(copy_cv_ref)

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int, char>::type, int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int, const char>::type, const int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int, volatile char>::type, volatile int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int, const volatile char>::type, const volatile int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int, char&>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int, const char&>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int, volatile char&>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int, const volatile char&>::type, const volatile int&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int, char>::type, const int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int, const char>::type, const int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int, volatile char>::type, const volatile int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int, const volatile char>::type, const volatile int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int, char&>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int, const char&>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int, volatile char&>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int, const volatile char&>::type, const volatile int&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int, char>::type, volatile int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int, const char>::type, const volatile int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int, volatile char>::type, volatile int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int, const volatile char>::type, const volatile int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int, char&>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int, const char&>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int, volatile char&>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int, const volatile char&>::type, const volatile int&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int, char>::type, const volatile int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int, const char>::type, const volatile int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int, volatile char>::type, const volatile int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int, const volatile char>::type, const volatile int);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int, char&>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int, const char&>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int, volatile char&>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int, const volatile char&>::type, const volatile int&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&, char>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&, const char>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&, volatile char>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&, const volatile char>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&, char&>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&, const char&>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&, volatile char&>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&, const volatile char&>::type, int&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&, char>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&, const char>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&, volatile char>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&, const volatile char>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&, char&>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&, const char&>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&, volatile char&>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&, const volatile char&>::type, const int&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&, char>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&, const char>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&, volatile char>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&, const volatile char>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&, char&>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&, const char&>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&, volatile char&>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&, const volatile char&>::type, volatile int&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&, char>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&, const char>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&, volatile char>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&, const volatile char>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&, char&>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&, const char&>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&, volatile char&>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&, const volatile char&>::type, const volatile int&);

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int, char&&>::type, int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int, const char&&>::type, const int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int, volatile char&&>::type, volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int, const volatile char&&>::type, const volatile int&&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int, char&&>::type, const int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int, const char&&>::type, const int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int, volatile char&&>::type, const volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int, const volatile char&&>::type, const volatile int&&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int, char&&>::type, volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int, const char&&>::type, const volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int, volatile char&&>::type, volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int, const volatile char&&>::type, const volatile int&&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int, char&&>::type, const volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int, const char&&>::type, const volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int, volatile char&&>::type, const volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int, const volatile char&&>::type, const volatile int&&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&, char&&>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&, const char&&>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&, volatile char&&>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&, const volatile char&&>::type, int&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&, char&&>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&, const char&&>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&, volatile char&&>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&, const volatile char&&>::type, const int&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&, char&&>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&, const char&&>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&, volatile char&&>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&, const volatile char&&>::type, volatile int&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&, char&&>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&, const char&&>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&, volatile char&&>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&, const volatile char&&>::type, const volatile int&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&&, char>::type, int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&&, const char>::type, int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&&, volatile char>::type, int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&&, const volatile char>::type, int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&&, char&>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&&, const char&>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&&, volatile char&>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&&, const volatile char&>::type, int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&&, char&&>::type, int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&&, const char&&>::type, int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&&, volatile char&&>::type, int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<int&&, const volatile char&&>::type, int&&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&&, char>::type, const int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&&, const char>::type, const int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&&, volatile char>::type, const int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&&, const volatile char>::type, const int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&&, char&>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&&, const char&>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&&, volatile char&>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&&, const volatile char&>::type, const int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&&, char&&>::type, const int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&&, const char&&>::type, const int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&&, volatile char&&>::type, const int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const int&&, const volatile char&&>::type, const int&&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&&, char>::type, volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&&, const char>::type, volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&&, volatile char>::type, volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&&, const volatile char>::type, volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&&, char&>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&&, const char&>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&&, volatile char&>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&&, const volatile char&>::type, volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&&, char&&>::type, volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&&, const char&&>::type, volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&&, volatile char&&>::type, volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<volatile int&&, const volatile char&&>::type, volatile int&&);

BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&&, char>::type, const volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&&, const char>::type, const volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&&, volatile char>::type, const volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&&, const volatile char>::type, const volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&&, char&>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&&, const char&>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&&, volatile char&>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&&, const volatile char&>::type, const volatile int&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&&, char&&>::type, const volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&&, const char&&>::type, const volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&&, volatile char&&>::type, const volatile int&&);
BOOST_CHECK_TYPE3(::tt::copy_cv_ref<const volatile int&&, const volatile char&&>::type, const volatile int&&);
#endif

TT_TEST_END
