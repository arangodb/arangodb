//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#include <boost/test/included/unit_test.hpp>
#include <boost/bind.hpp>
using namespace boost::unit_test;

class test_class
{
public:
  void test_method1()
  {
    BOOST_TEST( true /* test assertion */ );
  }
  void test_method2()
  {
    BOOST_TEST( false /* test assertion */ );
  }
};

test_suite* init_unit_test_suite( int /*argc*/, char* /*argv*/[] )
{
  boost::shared_ptr<test_class> tester( new test_class );

  framework::master_test_suite().
    add( BOOST_TEST_CASE( boost::bind( &test_class::test_method1, tester )));
  framework::master_test_suite().
    add( BOOST_TEST_CASE( boost::bind( &test_class::test_method2, tester )));
  return 0;
}
//]
