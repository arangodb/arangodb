// Copyright Abel Sinkovics (abel@sinkovics.hu) 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/int_.hpp>
#include <boost/metaparse/is_error.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_result.hpp>

#include "common.hpp"

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(int)
{
  using boost::metaparse::is_error;
  using boost::metaparse::int_;
  using boost::metaparse::start;
  using boost::metaparse::get_result;
  
  using boost::mpl::apply_wrap2;
  using boost::mpl::equal_to;

  // test_with_text
  BOOST_MPL_ASSERT((is_error<apply_wrap2<int_, str_hello, start> >));
  
  // test_with_zero
  BOOST_MPL_ASSERT((
    equal_to<get_result<apply_wrap2<int_, str_0, start> >::type, int0>
  ));

  // test_with_one_digit
  BOOST_MPL_ASSERT((
    equal_to<get_result<apply_wrap2<int_, str_1, start> >::type, int1>
  ));

  // test_with_big_number
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<apply_wrap2<int_, str_1983, start> >::type,
      boost::mpl::int_<1983>
    >
  ));
  
  // test_with_empty_string
  BOOST_MPL_ASSERT((is_error<apply_wrap2<int_, str_, start> >));
}

