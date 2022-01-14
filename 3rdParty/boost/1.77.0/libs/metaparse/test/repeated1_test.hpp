// Copyright Abel Sinkovics (abel@sinkovics.hu) 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// This header file contains code that is reused by other cpp files

#include <boost/metaparse/letter.hpp>
#include <boost/metaparse/is_error.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_result.hpp>
#include <boost/metaparse/always.hpp>
#include <boost/metaparse/one_char.hpp>

#include "common.hpp"
 
#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/equal.hpp>
#include <boost/mpl/vector_c.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(TEST_NAME)
{
  using boost::metaparse::get_result;
  using boost::metaparse::letter;
  using boost::metaparse::start;
  using boost::metaparse::is_error;
  using boost::metaparse::always;
  using boost::metaparse::one_char;
  
  using boost::mpl::equal;
  using boost::mpl::apply_wrap2;
  using boost::mpl::list;
  using boost::mpl::vector_c;
  using boost::mpl::vector;

  typedef repeated1<letter> repeated1_letter;
  typedef always<one_char, int> always_int;

  // test_empty_input
  BOOST_MPL_ASSERT((is_error<apply_wrap2<repeated1_letter, str_, start> >));
  
  // test0
  BOOST_MPL_ASSERT((is_error<apply_wrap2<repeated1_letter, chars0, start> >));
  
  // test1
  BOOST_MPL_ASSERT((
    equal<
      get_result<apply_wrap2<repeated1_letter, chars1, start> >::type,
      vector_c<char, 'h'>
    >
  ));
  
  // test2
  BOOST_MPL_ASSERT((
    equal<
      get_result<apply_wrap2<repeated1_letter, chars2, start> >::type,
      vector_c<char, 'h', 'e'>
    >
  ));
  
  // test3
  BOOST_MPL_ASSERT((
    equal<
      get_result<apply_wrap2<repeated1_letter, chars3, start> >::type,
      vector_c<char, 'h', 'e', 'l'>
    >
  ));

  // test4
  BOOST_MPL_ASSERT((
    equal<
      get_result<apply_wrap2<repeated1_letter, chars4, start> >::type,
      vector_c<char, 'h', 'e', 'l', 'l'>
    >
  ));
  
  // test5
  BOOST_MPL_ASSERT((
    equal<
      get_result<apply_wrap2<repeated1_letter, chars5, start> >::type,
      vector_c<char, 'h', 'e', 'l', 'l', 'o'>
    >
  ));

  // test_no_extra_evaluation
  BOOST_MPL_ASSERT((
    equal<
      get_result<apply_wrap2<repeated1<always_int>, str_ca, start> >::type,
      vector<int, int>
    >
  ));
}


