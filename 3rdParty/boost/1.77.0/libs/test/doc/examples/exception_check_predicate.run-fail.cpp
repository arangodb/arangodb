//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>

struct my_exception
{
  explicit my_exception( int ec = 0 ) : m_error_code( ec )
  {}

  int m_error_code;
};

bool is_critical( my_exception const& ex ) { return ex.m_error_code < 0; }

void some_func( int i ) { if( i>=0 ) throw my_exception( i ); }

BOOST_AUTO_TEST_CASE( test_exception_predicate )
{
  BOOST_CHECK_EXCEPTION( some_func(0), my_exception, !is_critical );
  BOOST_CHECK_EXCEPTION( some_func(1), my_exception, is_critical );
}
//]
