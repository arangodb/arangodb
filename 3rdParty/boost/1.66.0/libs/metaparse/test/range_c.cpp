// Copyright Abel Sinkovics (abel@sinkovics.hu) 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/range_c.hpp>
#include <boost/metaparse/is_error.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_result.hpp>

#include "common.hpp"

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/mpl/char.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(range_c)
{
  using boost::metaparse::is_error;
  using boost::metaparse::range_c;
  using boost::metaparse::start;
  using boost::metaparse::get_result;
  
  using boost::mpl::apply_wrap2;
  using boost::mpl::equal_to;
  using boost::mpl::char_;

  typedef range_c<'0', '9'> digit_range;

  // test_with_text
  BOOST_MPL_ASSERT((is_error<apply_wrap2<digit_range, str_hello, start> >));
  
  // test_with_number
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<apply_wrap2<digit_range, str_1983, start> >::type,
      char_1
    >
  ));
  
  // test_with_empty_string
  BOOST_MPL_ASSERT((is_error<apply_wrap2<digit_range, str_, start> >));
}

