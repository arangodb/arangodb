
//  (C) Copyright John Maddock 2000.
//  (C) Copyright Peter Dimov 2018.
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/is_noncopyable.hpp>
#endif
#include <boost/core/noncopyable.hpp>
#include "test.hpp"
#include "check_integral_constant.hpp"

struct X
{
};

class Y: private boost::noncopyable
{
};

class Z: private Y
{
};

TT_TEST_BEGIN(is_noncopyable)

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_noncopyable<void>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_noncopyable<const void>::value, false);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_noncopyable<int>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_noncopyable<const int>::value, false);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_noncopyable<X>::value, false);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_noncopyable<const X>::value, false);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_noncopyable<Y>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_noncopyable<const Y>::value, true);

BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_noncopyable<Z>::value, true);
BOOST_CHECK_INTEGRAL_CONSTANT(::tt::is_noncopyable<const Z>::value, true);

TT_TEST_END
