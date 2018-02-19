//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <boost/test/parameterized_test.hpp>
#include <boost/bind.hpp>
using namespace boost::unit_test;
namespace tt=boost::test_tools;
using namespace boost;

class test_class {
public:
  void test_method( double d )
  {
    BOOST_TEST( d * 100 == (double)(int)(d*100), tt::tolerance(0.0001) );
  }
} tester;

bool init_unit_test()
{
  double params[] = { 1., 1.1, 1.01, 1.001, 1.0001 };

  boost::function<void (double)> test_method = bind( &test_class::test_method, &tester, _1);

  framework::master_test_suite().
    add( BOOST_PARAM_TEST_CASE( test_method, params, params+5 ) );

  return true;
}
//]
