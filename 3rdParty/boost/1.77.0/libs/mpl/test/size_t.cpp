
// Copyright Aleksey Gurtovoy 2001-2004
//
// Distributed under the Boost Software License, Version 1.0. 
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/mpl for documentation.

// $Id$
// $Date$
// $Revision$

// Necessary to overcome a strange name lookup bug in GCC 3.3 and 4.0 for Mac OS X
#if defined(__APPLE_CC__) && defined(__GNUC__) && (__GNUC__ <= 4)
#  include <cassert>
#endif

#include <boost/mpl/size_t.hpp>
#include <boost/mpl/identity.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>

#include "integral_wrapper_test.hpp"

MPL_TEST_CASE()
{
#   define WRAPPER(T, i) mpl::size_t<i>
    BOOST_PP_REPEAT_FROM_TO(1, 11, INTEGRAL_WRAPPER_TEST, std::size_t)
}
