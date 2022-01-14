// Copyright Abel Sinkovics (abel@sinkovics.hu) 2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/is_error.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_result.hpp>
#include <boost/metaparse/get_remaining.hpp>
#include <boost/metaparse/get_position.hpp>
#include <boost/metaparse/get_message.hpp>
#include <boost/metaparse/error/literal_expected.hpp>

#include "common.hpp"

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/assert.hpp>

#include <boost/type_traits/is_same.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(lit_c)
{
  using boost::metaparse::get_result;
  using boost::metaparse::start;
  using boost::metaparse::is_error;
  using boost::metaparse::get_remaining;
  using boost::metaparse::get_position;
  using boost::metaparse::get_message;
  
  using boost::metaparse::error::literal_expected;

  using boost::mpl::equal_to;
  using boost::mpl::apply_wrap2;

  using boost::is_same;

  // test_accept
  BOOST_MPL_ASSERT((
    equal_to<get_result<apply_wrap2<lit_c_h, str_hello, start> >::type, char_h>
  ));

  // test_reject
  BOOST_MPL_ASSERT((is_error<apply_wrap2<lit_c_h, str_bello, start> >));

  // test_with_empty_string
  BOOST_MPL_ASSERT((is_error< apply_wrap2<lit_c_h, str_, start> >));

  // test_error_with_empty_string
  BOOST_MPL_ASSERT((
    is_same<
      literal_expected<'h'>,
      get_message<apply_wrap2<lit_c_h, str_, start> >::type
    >
  ));

  // test_remaining_string
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<
        apply_wrap2<
          lit_c_e,
          get_remaining<apply_wrap2<lit_c_h, str_hello, start> >::type,
          get_position<apply_wrap2<lit_c_h, str_hello, start> >::type
        >
      >::type,
      char_e
    >
  ));
}


