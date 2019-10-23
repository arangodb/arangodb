// Copyright Abel Sinkovics (abel@sinkovics.hu) 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/one_of_c.hpp>
#include <boost/metaparse/is_error.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_result.hpp>

#include "common.hpp"

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(one_of_c)
{
  using boost::metaparse::is_error;
  using boost::metaparse::one_of_c;
  using boost::metaparse::start;
  using boost::metaparse::get_result;
  
  using boost::mpl::apply_wrap2;
  using boost::mpl::equal_to;
  
  // test0
  BOOST_MPL_ASSERT((is_error<apply_wrap2<one_of_c< >, str_hello, start> >));
  
  // test_1_with_good
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<apply_wrap2<one_of_c<'h'>, str_hello, start> >::type,
      char_h
    >
  ));

  // test_1_with_bad
  BOOST_MPL_ASSERT((
    is_error<apply_wrap2<one_of_c<'x'>, str_hello, start> >
  ));

  // test_2_with_two_good
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<apply_wrap2<one_of_c<'h', 'x'>, str_hello, start> >::type,
      char_h
    >
  ));

  // test_2_with_first_good
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<apply_wrap2<one_of_c<'h', 'x'>, str_hello, start> >::type,
      char_h
    >
  ));

  // test_2_with_second_good
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<apply_wrap2<one_of_c<'x', 'h'>, str_hello, start> >::type,
      char_h
    >
  ));

  // test_2_with_two_bad
  BOOST_MPL_ASSERT((
    is_error<apply_wrap2<one_of_c<'x', 'y'>, str_hello, start> >
  ));
}


