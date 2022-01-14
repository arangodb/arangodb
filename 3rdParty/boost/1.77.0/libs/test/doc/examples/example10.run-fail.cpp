//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>
#include <boost/mpl/list.hpp>

typedef boost::mpl::list<int,long,unsigned char> test_types;

BOOST_AUTO_TEST_CASE_TEMPLATE( my_test, T, test_types )
{
  BOOST_TEST( sizeof(T) == (unsigned)4 );
}

typedef std::tuple<int, long, unsigned char> test_types_w_tuples;

BOOST_AUTO_TEST_CASE_TEMPLATE( my_tuple_test, T, test_types_w_tuples )
{
  BOOST_TEST( sizeof(T) == (unsigned)4 );
}
//]
