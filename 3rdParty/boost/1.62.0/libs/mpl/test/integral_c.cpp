
// Copyright Aleksey Gurtovoy 2001-2004
//
// Distributed under the Boost Software License,Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$

#include <boost/mpl/integral_c.hpp>
#include <boost/preprocessor/repeat.hpp>

#include "integral_wrapper_test.hpp"


MPL_TEST_CASE()
{
#   define WRAPPER(T, i) integral_c<T,i>

#if !(defined(__APPLE_CC__) && defined(__GNUC__) && (__GNUC__ == 3) && (__GNUC_MINOR__ <= 3))
    BOOST_PP_REPEAT(10, INTEGRAL_WRAPPER_TEST, char)
#endif
    BOOST_PP_REPEAT(10, INTEGRAL_WRAPPER_TEST, short)
    BOOST_PP_REPEAT(10, INTEGRAL_WRAPPER_TEST, int)
    BOOST_PP_REPEAT(10, INTEGRAL_WRAPPER_TEST, long)
}
