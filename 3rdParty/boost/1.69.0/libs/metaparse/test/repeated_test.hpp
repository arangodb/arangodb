// Copyright Abel Sinkovics (abel@sinkovics.hu) 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// This header file contains code that is reused by other cpp files

#include <boost/metaparse/letter.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_result.hpp>
#include <boost/metaparse/always.hpp>
#include <boost/metaparse/one_char.hpp>

#include "common.hpp"
 
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/equal.hpp>
#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/vector_c.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(TEST_NAME)
{
  using boost::metaparse::get_result;
  using boost::metaparse::letter;
  using boost::metaparse::start;
  using boost::metaparse::one_char;
  using boost::metaparse::always;
  
  using boost::mpl::equal;
  using boost::mpl::equal_to;
  using boost::mpl::apply_wrap2;
  using boost::mpl::vector_c;
  using boost::mpl::vector;
  using boost::mpl::list;
  using boost::mpl::size;
  
  typedef repeated<letter> repeated_letter;
  typedef always<one_char, int> always_int;

  // test_empty_input
  BOOST_MPL_ASSERT((
    equal<get_result<apply_wrap2<repeated_letter, str_, start> >::type, list<> >
  ));
  
  // test0
  BOOST_MPL_ASSERT((
    equal<
      get_result<apply_wrap2<repeated_letter, chars0, start> >::type,
      list<>
    >
  ));
  
  // test1
  BOOST_MPL_ASSERT((
    equal<
      get_result<apply_wrap2<repeated_letter, chars1, start> >::type,
      vector_c<char, 'h'>
    >
  ));
  
  // test2
  BOOST_MPL_ASSERT((
    equal<
      get_result<apply_wrap2<repeated_letter, chars2, start> >::type,
      vector_c<char, 'h', 'e'>
    >
  ));
  
  // test3
  BOOST_MPL_ASSERT((
    equal<
      get_result<apply_wrap2<repeated_letter, chars3, start> >::type,
      vector_c<char, 'h', 'e', 'l'>
    >
  ));
  
  // test4
  BOOST_MPL_ASSERT((
    equal<
      get_result<apply_wrap2<repeated_letter, chars4, start> >::type,
      vector_c<char, 'h', 'e', 'l', 'l'>
    >
  ));
  
  // test5
  BOOST_MPL_ASSERT((
    equal<
      get_result<apply_wrap2<repeated_letter, chars5, start> >::type,
      vector_c<char, 'h', 'e', 'l', 'l', 'o'>
    >
  ));
  
  // test_length
  BOOST_MPL_ASSERT((
    equal_to<
      size<
        get_result<apply_wrap2<repeated_letter, chars3, start> >::type
      >::type,
      int3
    >
  ));

  // test_no_extra_evaluation
  BOOST_MPL_ASSERT((
    equal<
      get_result<apply_wrap2<repeated<always_int>, str_ca, start> >::type,
      vector<int, int>
    >
  ));
}


