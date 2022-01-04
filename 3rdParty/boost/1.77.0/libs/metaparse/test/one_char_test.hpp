// Copyright Abel Sinkovics (abel@sinkovics.hu) 2010 - 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// This header file contains code that is reused by other cpp files

#include <boost/metaparse/is_error.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_result.hpp>
#include <boost/metaparse/get_remaining.hpp>
#include <boost/metaparse/get_position.hpp>
#include <boost/metaparse/get_line.hpp>
#include <boost/metaparse/iterate_c.hpp>

#include "common.hpp"

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

namespace
{
  using boost::metaparse::start;
  
  using boost::mpl::list_c;
  using boost::mpl::apply_wrap2;

  typedef list_c<char, 'a','\n','b'> unix_multi_line_text;
  typedef list_c<char, 'a','\r','\n','b'> dos_multi_line_text;
  typedef list_c<char, 'a','\r','b'> mac_multi_line_text;

  typedef apply_wrap2<oc, str_hello, start> parse_first_char;
}

BOOST_METAPARSE_TEST_CASE(TEST_NAME)
{
  using boost::metaparse::get_result;
  using boost::metaparse::get_remaining;
  using boost::metaparse::get_position;
  using boost::metaparse::is_error;
  using boost::metaparse::iterate_c;
  using boost::metaparse::get_line;
  
  using boost::mpl::equal_to;

  // test_for_non_empty_string
  BOOST_MPL_ASSERT((equal_to<get_result<parse_first_char>::type, char_h>));

  // test_for_non_empty_string_second
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<
        apply_wrap2<
          oc,
          get_remaining<parse_first_char>::type,
          get_position<parse_first_char>::type
        >
      >::type,
      char_e
    >
  ));

  // test_for_empty_string
  BOOST_MPL_ASSERT((is_error<apply_wrap2<oc, str_, start> >));
  
  // test_unix_multi_line_text
  BOOST_MPL_ASSERT((
    equal_to<
      int2,
      get_line<
        get_position<
          apply_wrap2<iterate_c<oc, 2>, unix_multi_line_text, start>
        >
      >
    >
  ));

  // test_dos_multi_line_text
  BOOST_MPL_ASSERT((
    equal_to<
      int2,
      get_line<
        get_position<apply_wrap2<iterate_c<oc, 3>, dos_multi_line_text, start> >
      >
    >
  ));

  // test_mac_multi_line_text
  BOOST_MPL_ASSERT((
    equal_to<
      int2,
      get_line<
        get_position<apply_wrap2<iterate_c<oc, 2>, mac_multi_line_text, start> >
      >
    >
  ));
}



