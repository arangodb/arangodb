//  (C) Copyright Gennadiy Rozental 2001-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  File        : $RCSfile$
//
//  Version     : $Revision$
//
//  Description : header-only usage variant with custom init test
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_CASE( test )
{
    int i = 1;
    BOOST_CHECK( i*i == 1 );
}

//____________________________________________________________________________//

bool init_unit_test() {
    boost::unit_test::framework::master_test_suite().p_name.value = "Custom init func";

    return true;
}

//____________________________________________________________________________//

// EOF
