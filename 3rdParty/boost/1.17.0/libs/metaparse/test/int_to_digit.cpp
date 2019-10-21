// Copyright Abel Sinkovics (abel@sinkovics.hu) 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/util/int_to_digit.hpp>

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/char.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(int_to_digit)
{
  using boost::metaparse::util::int_to_digit;

  using boost::mpl::char_;
  using boost::mpl::int_;
  using boost::mpl::equal_to;
  
  // test0
  BOOST_MPL_ASSERT((equal_to<char_<'0'>, int_to_digit<int_<0> >::type>));

  // test5
  BOOST_MPL_ASSERT((equal_to<char_<'5'>, int_to_digit<int_<5> >::type>));

  // test9
  BOOST_MPL_ASSERT((equal_to<char_<'9'>, int_to_digit<int_<9> >::type>));
}


