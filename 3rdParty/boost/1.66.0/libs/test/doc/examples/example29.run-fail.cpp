//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>
#include <boost/test/output_test_stream.hpp> 
using boost::test_tools::output_test_stream;

BOOST_AUTO_TEST_CASE( test )
{
  output_test_stream output( "pattern_file", true );
  int i=2;
  output << "i=" << i;
  BOOST_TEST( output.match_pattern() );

  output << "\nFile: " << __FILE__ << " Line:YYY";
  BOOST_TEST( output.match_pattern() ); /*< This line generates the error Line:YYY != Line:XXX >*/
}
//]
