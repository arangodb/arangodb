// Copyright Abel Sinkovics (abel@sinkovics.hu) 2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/digit.hpp>
#include <boost/metaparse/is_error.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_result.hpp>
#include <boost/metaparse/get_message.hpp>

#include <boost/metaparse/error/digit_expected.hpp>

#include "common.hpp"

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

#include <boost/type_traits/is_same.hpp>

BOOST_METAPARSE_TEST_CASE(digit)
{
  using boost::metaparse::is_error;
  using boost::metaparse::digit;
  using boost::metaparse::start;
  using boost::metaparse::get_result;
  using boost::metaparse::get_message;

  using boost::metaparse::error::digit_expected;
  
  using boost::mpl::apply_wrap2;
  using boost::mpl::equal_to;

  using boost::is_same;

  // test_with_text
  BOOST_MPL_ASSERT((is_error<apply_wrap2<digit, str_hello, start> >));
  
  // test_with_number
  BOOST_MPL_ASSERT((
    equal_to<get_result<apply_wrap2<digit, str_1983, start> >::type, char_1>
  ));
  
  // test_with_empty_string
  BOOST_MPL_ASSERT((is_error<apply_wrap2<digit, str_, start> >));

  // test_error_message_with_empty_string
  BOOST_MPL_ASSERT((
    is_same<digit_expected, get_message<apply_wrap2<digit, str_, start> >::type>
  ));
}

