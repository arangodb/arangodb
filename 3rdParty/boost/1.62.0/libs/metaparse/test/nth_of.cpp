// Copyright Abel Sinkovics (abel@sinkovics.hu) 2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/nth_of.hpp>
#include <boost/metaparse/nth_of_c.hpp>
#include <boost/metaparse/is_error.hpp>
#include <boost/metaparse/get_result.hpp>
#include <boost/metaparse/start.hpp>

#include "common.hpp"

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(nth_of)
{
  using boost::metaparse::get_result;
  using boost::metaparse::nth_of_c;
  using boost::metaparse::start;
  using boost::metaparse::nth_of;
  using boost::metaparse::is_error;
  
  using boost::mpl::equal_to;
  using boost::mpl::apply_wrap2;

  namespace mpl = boost::mpl;
  namespace mp = boost::metaparse;

  // test_first_of_one
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<apply_wrap2<nth_of_c<0, lit_h>, str_hello, start> >::type,
      char_h
    >
  ));

  // test_first_of_two
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<
        apply_wrap2<nth_of_c<0, lit_h, lit_e>, str_hello, start>
      >::type,
      char_h
    >
  ));

  // test_second_of_two
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<
        apply_wrap2<nth_of<int1, lit_h, lit_e>, str_hello, start>
      >::type,
      char_e
    >
  ));

  // test_nothing
  BOOST_MPL_ASSERT((
    is_error<apply_wrap2<nth_of_c<1, lit_x, lit_e>, str_hello, start> >
  ));
  
  // test_first_of_none
  BOOST_MPL_ASSERT((
    is_error<apply_wrap2<nth_of_c<0>, str_hello, start> >
  ));

  // test_n_is_less_than_zero
  BOOST_MPL_ASSERT((
    is_error<apply_wrap2<nth_of_c<-1, lit_h, lit_e>, str_hello, start> >
  ));

  // test_n_is_greater_than_the_number_of_parsers
  BOOST_MPL_ASSERT((
    is_error<apply_wrap2<nth_of_c<2, lit_h, lit_e>, str_hello, start> >
  ));

  // test_error_before_the_nth
  BOOST_MPL_ASSERT((
    is_error<apply_wrap2<nth_of_c<1, lit_x, lit_e, lit_l>, str_hello, start> >
  ));

  // test_error_at_the_nth
  BOOST_MPL_ASSERT((
    is_error<apply_wrap2<nth_of_c<1, lit_h, lit_x, lit_l>, str_hello, start> >
  ));

  // test_error_after_the_nth
  BOOST_MPL_ASSERT((
    is_error<apply_wrap2<nth_of_c<1, lit_h, lit_e, lit_x>, str_hello, start> >
  ));
}

