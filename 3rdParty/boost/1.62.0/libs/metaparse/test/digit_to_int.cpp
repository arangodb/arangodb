// Copyright Abel Sinkovics (abel@sinkovics.hu) 2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/util/digit_to_int.hpp>
#include <boost/metaparse/error/digit_expected.hpp>

#include "common.hpp"

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/mpl/assert.hpp>

#include <boost/type_traits/is_same.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(digit_to_int)
{
  using boost::metaparse::util::digit_to_int;
  using boost::metaparse::error::digit_expected;
  
  using boost::mpl::equal_to;
  using boost::mpl::apply_wrap1;
  using boost::mpl::true_;

  using boost::is_same;

  // test0
  BOOST_MPL_ASSERT((equal_to<apply_wrap1<digit_to_int<>, char_0>::type, int0>));

  // test9
  BOOST_MPL_ASSERT((equal_to<apply_wrap1<digit_to_int<>, char_9>::type, int9>));
  
  // test_error
  BOOST_MPL_ASSERT((
    is_same<digit_expected, apply_wrap1<digit_to_int<>, char_x>::type>
  ));
}

