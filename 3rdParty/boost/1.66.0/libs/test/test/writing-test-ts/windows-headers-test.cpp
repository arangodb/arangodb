//  (C) Copyright Raffi Enficiaud 2015
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//  See http://www.boost.org/libs/test for the library home page.
//
//  Tests some compilation troubleshooting issues with the windows headers (eg min/max macros)
// ***************************************************************************

#define BOOST_TEST_MODULE test_windows_headers

#include <boost/config.hpp>
#ifdef BOOST_WINDOWS
#include <windows.h>
#endif

#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp> // Extra test tool for FP comparison.

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES( test, 2 )

BOOST_AUTO_TEST_CASE( test )
{
    // produces an error
    BOOST_TEST(1 == 0);

    // this is added in order to compile floating point relative code as well
    // which might have trouble compiling if system headers are included (as for
    // boost.thread, eg. for std::min).
    BOOST_CHECK_CLOSE(1.1, 1.2, 1e-5);
}
