// Copyright Abel Sinkovics (abel@sinkovics.hu) 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/source_position.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/next_char.hpp>
#include <boost/metaparse/next_line.hpp>
#include <boost/metaparse/get_prev_char.hpp>
#include <boost/metaparse/get_line.hpp>
#include <boost/metaparse/get_col.hpp>

#include "common.hpp"

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/not_equal_to.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/mpl/less.hpp>
#include <boost/mpl/greater.hpp>
#include <boost/mpl/greater_equal.hpp>
#include <boost/mpl/less_equal.hpp>
#include <boost/mpl/not.hpp>

#include "test_case.hpp"

namespace
{
  using boost::metaparse::source_position;
  using boost::metaparse::next_char;
  using boost::metaparse::start;

  typedef source_position<int11, int13, int1> sp;
  typedef next_char<start, char_0> next0;
 }

BOOST_METAPARSE_TEST_CASE(source_position)
{
  using boost::metaparse::get_line;
  using boost::metaparse::get_col;
  using boost::metaparse::get_prev_char;
  using boost::metaparse::next_line;
  
  using boost::mpl::equal_to;
  using boost::mpl::not_equal_to;

  using boost::mpl::not_;
  using boost::mpl::less;
  using boost::mpl::greater;
  using boost::mpl::greater_equal;
  using boost::mpl::less_equal;
  
  // test_get_line
  BOOST_MPL_ASSERT((equal_to<int11, get_line<sp>::type>));

  // test_get_col
  BOOST_MPL_ASSERT((equal_to<int13, get_col<sp>::type>));
  
  // test_get_prev_char
  BOOST_MPL_ASSERT((equal_to<int1, get_prev_char<sp>::type>));

  // test_line_of_start
  BOOST_MPL_ASSERT((equal_to<int1, get_line<start>::type>));

  // test_col_of_start
  BOOST_MPL_ASSERT((equal_to<int1, get_col<start>::type>));

  // test_next_chars_char
  BOOST_MPL_ASSERT((equal_to<int2, get_col<next0>::type>));

  // test_next_chars_line
  BOOST_MPL_ASSERT((equal_to<int1, get_line<next0>::type>));

  // test_next_lines_char
  BOOST_MPL_ASSERT((
    equal_to<int1, get_col<next_line<next0, char_0> >::type>
  ));

  // test_next_lines_line
  BOOST_MPL_ASSERT((
    equal_to<int2, get_line<next_line<start, char_0> >::type>
  ));

  // test_next_chars_prev_char
  BOOST_MPL_ASSERT((
    equal_to<char_1, get_prev_char< next_char<start, char_1> >::type>
  ));

  // test_next_lines_prev_char
  BOOST_MPL_ASSERT((
    equal_to<char_1, get_prev_char<next_line<start, char_1> >::type>
  ));

  // test_equal_source_positions
  BOOST_MPL_ASSERT((equal_to<sp, sp>));

  // test_equal_source_positions_when_prev_char_is_different
  BOOST_MPL_ASSERT((
    not_<
      equal_to<
        source_position<int11, int13, char_a>,
        source_position<int11, int13, char_b>
      >::type
    >
  ));

  // test_not_equal_source_positions_when_line_is_different
  BOOST_MPL_ASSERT((
    not_equal_to<
      source_position<int11, int13, char_a>,
      source_position<int13, int13, char_a>
    >
  ));

  // test_not_equal_source_positions_when_col_is_different
  BOOST_MPL_ASSERT((
    not_equal_to<
      source_position<int11, int11, char_a>,
      source_position<int11, int13, char_a>
    >
  ));

  // test_source_position_is_not_less_than_itself
  BOOST_MPL_ASSERT((
    not_<
      less<
        source_position<int11, int13, char_a>,
        source_position<int11, int13, char_a>
      >::type
    >
  ));

  // test_source_position_earlier_in_the_same_line_is_less
  BOOST_MPL_ASSERT((
    less<
      source_position<int11, int11, char_a>,
      source_position<int11, int13, char_a>
    >
  ));

  // test_source_position_later_in_the_same_line_is_not_less
  BOOST_MPL_ASSERT((
    not_<
      less<
        source_position<int11, int13, char_a>,
        source_position<int11, int11, char_a>
      >::type
    >
  ));

  // test_source_position_in_the_same_pos_with_less_char_is_less
  BOOST_MPL_ASSERT((
    less<
      source_position<int11, int13, char_a>,
      source_position<int11, int13, char_b>
    >
  ));

  // test_source_position_earlier_line_is_less
  BOOST_MPL_ASSERT((
    less<
      source_position<int1, int28, char_a>,
      source_position<int11, int13, char_a>
    >
  ));

  // test_source_position_later_line_is_not_less
  BOOST_MPL_ASSERT((
    not_<
      less<
        source_position<int28, int2, char_a>,
        source_position<int11, int13, char_a>
      >::type
    >
  ));

  // test_source_position_is_greater_equal_to_itself
  BOOST_MPL_ASSERT((
    greater_equal<
      source_position<int11, int13, char_a>,
      source_position<int11, int13, char_a>
    >
  ));

  // test_source_position_earlier_in_the_same_line_is_not_greater_equal
  BOOST_MPL_ASSERT((
    not_<
      greater_equal<
        source_position<int11, int11, char_a>,
        source_position<int11, int13, char_a>
      >::type
    >
  ));

  // test_source_position_later_in_the_same_line_is_greater_equal
  BOOST_MPL_ASSERT((
    greater_equal<
      source_position<int11, int13, char_a>,
      source_position<int11, int11, char_a>
    >
  ));

  // test_source_position_in_the_same_pos_with_less_char_is_not_greater_equal
  BOOST_MPL_ASSERT((
    not_<
      greater_equal<
        source_position<int11, int13, char_a>,
        source_position<int11, int13, char_b>
      >::type
    >
  ));

  // test_source_position_earlier_line_is_not_greater_equal
  BOOST_MPL_ASSERT((
    not_<
      greater_equal<
        source_position<int1, int28, char_a>,
        source_position<int11, int13, char_a>
      >::type
    >
  ));

  // test_source_position_later_line_is_greater_equal
  BOOST_MPL_ASSERT((
    greater_equal<
      source_position<int28, int2, char_a>,
      source_position<int11, int13, char_a>
    >
  ));

  // test_source_position_is_not_greater_than_itself
  BOOST_MPL_ASSERT((
    not_<
      greater<
        source_position<int11, int13, char_a>,
        source_position<int11, int13, char_a>
      >::type
    >
  ));

  // test_source_position_earlier_in_the_same_line_is_not_greater
  BOOST_MPL_ASSERT((
    not_<
      greater<
        source_position<int11, int11, char_a>,
        source_position<int11, int13, char_a>
      >::type
    >
  ));

  // test_source_position_later_in_the_same_line_is_greater
  BOOST_MPL_ASSERT((
    greater<
      source_position<int11, int13, char_a>,
      source_position<int11, int11, char_a>
    >
  ));

  // test_source_position_in_the_same_pos_with_less_char_is_not_greater
  BOOST_MPL_ASSERT((
    not_<
      greater<
        source_position<int11, int13, char_a>,
        source_position<int11, int13, char_b>
      >::type
    >
  ));

  // test_source_position_earlier_line_is_not_greater
  BOOST_MPL_ASSERT((
    not_<
      greater<
        source_position<int1, int28, char_a>,
        source_position<int11, int13, char_a>
      >::type
    >
  ));

  // test_source_position_later_line_is_greater
  BOOST_MPL_ASSERT((
    greater<
      source_position<int28, int2, char_a>,
      source_position<int11, int13, char_a>
    >
  ));

  // test_source_position_is_less_equal_to_itself
  BOOST_MPL_ASSERT((
    less_equal<
      source_position<int11, int13, char_a>,
      source_position<int11, int13, char_a>
    >
  ));

  // test_source_position_earlier_in_the_same_line_is_less_equal
  BOOST_MPL_ASSERT((
    less_equal<
      source_position<int11, int11, char_a>,
      source_position<int11, int13, char_a>
    >
  ));

  // test_source_position_later_in_the_same_line_is_not_less_equal
  BOOST_MPL_ASSERT((
    not_<
      less_equal<
        source_position<int11, int13, char_a>,
        source_position<int11, int11, char_a>
      >::type
    >
  ));

  // test_source_position_in_the_same_pos_with_less_char_is_less_equal
  BOOST_MPL_ASSERT((
    less_equal<
      source_position<int11, int13, char_a>,
      source_position<int11, int13, char_b>
    >
  ));

  // test_source_position_earlier_line_is_less_equal
  BOOST_MPL_ASSERT((
    less_equal<
      source_position<int1, int28, char_a>,
      source_position<int11, int13, char_a>
    >
  ));

  // test_source_position_later_line_is_not_less_equal
  BOOST_MPL_ASSERT((
    not_<
      less_equal<
        source_position<int28, int2, char_a>,
        source_position<int11, int13, char_a>
      >::type
    >
  ));
}

