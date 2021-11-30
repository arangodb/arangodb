//  (C) Copyright Gennadiy Rozental 2001-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  Description : header-only usage variant with multiple translation units test 
// ***************************************************************************

// this test requires that the linking has not been manipulated
// Since this is way impossible to make it work from B2, the undef of the
// auto linking variables is done here in the source.
// should be done before any include and in all translation units
#if defined(BOOST_TEST_NO_LIB)
#undef BOOST_TEST_NO_LIB
#endif

#if defined(BOOST_ALL_NO_LIB)
#undef BOOST_ALL_NO_LIB
#endif

#define BOOST_TEST_MODULE header-only multiunit test
#include <boost/test/included/unit_test.hpp>

#if defined(BOOST_TEST_NO_LIB) || defined(BOOST_ALL_NO_LIB)
#error BOOST_TEST_NO_LIB/BOOST_ALL_NO_LIB defined (post boost inclusion)!!
#endif

BOOST_AUTO_TEST_CASE( test1 )
{
    int i = 1;
    BOOST_CHECK( i*i == 1 );
}

//____________________________________________________________________________//

// EOF
