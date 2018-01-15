// Copyright Abel Sinkovics (abel@sinkovics.hu) 2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/keyword.hpp>
#include <boost/metaparse/is_error.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_result.hpp>
#include <boost/metaparse/get_remaining.hpp>

#include "common.hpp"

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/equal.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

namespace
{
  using boost::metaparse::keyword;

  using boost::mpl::list_c;

  typedef
    list_c<char, 'h','e','l','l','o',' ','w','o','r','l','d'>
    str_hello_world;
    
  typedef list_c<char, 'h','e','l','l','x'> str_hellx;
  typedef list_c<char, 'h','x','l','l','o'> str_hxllo;
  
  typedef keyword<str_hello> keyword_hello;
}

BOOST_METAPARSE_TEST_CASE(keyword)
{
  using boost::metaparse::get_result;
  using boost::metaparse::start;
  using boost::metaparse::is_error;
  using boost::metaparse::get_remaining;
  
  using boost::mpl::equal_to;
  using boost::mpl::apply_wrap2;
  using boost::mpl::equal;

  // test_result_type
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<
        apply_wrap2<keyword<str_hello, char_l>, str_hello, start>
      >::type,
      char_l
    >
  ));

  // test_empty_keyword
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<apply_wrap2<keyword<str_, char_l>, str_hello, start> >::type,
      char_l
    >
  ));

  // test_empty_input
  BOOST_MPL_ASSERT((is_error<apply_wrap2<keyword_hello, str_, start> >));

  // test_itself
  BOOST_MPL_ASSERT((
    equal<
      get_remaining<apply_wrap2<keyword_hello, str_hello, start> >::type,
      str_
    >
  ));

  // test_more_than_itself
  BOOST_MPL_ASSERT((
    equal<
      get_remaining<apply_wrap2<keyword_hello, str_hello_world, start> >::type,
      list_c<char, ' ', 'w', 'o', 'r', 'l', 'd'>
    >
  ));

  // test_no_match_at_end
  BOOST_MPL_ASSERT((is_error<apply_wrap2<keyword_hello, str_hellx, start> >));

  // test_no_match_in_the_middle
  BOOST_MPL_ASSERT((is_error<apply_wrap2<keyword_hello, str_hxllo, start> >));
}

