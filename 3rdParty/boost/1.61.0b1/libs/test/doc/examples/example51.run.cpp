//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>
#include <boost/test/unit_test_parameters.hpp>
using namespace boost::unit_test;

BOOST_AUTO_TEST_CASE( test_case0 )
{
  if( runtime_config::get<log_level>( runtime_config::LOG_LEVEL ) < log_warnings )
    unit_test_log.set_threshold_level( log_warnings );

  BOOST_WARN( sizeof(int) > 4 );
}
//]
