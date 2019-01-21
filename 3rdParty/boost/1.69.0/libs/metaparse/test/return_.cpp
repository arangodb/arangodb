// Copyright Abel Sinkovics (abel@sinkovics.hu) 2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/return_.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_result.hpp>
#include <boost/metaparse/get_remaining.hpp>
#include <boost/metaparse/get_position.hpp>

#include "common.hpp"

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

namespace
{
  using boost::metaparse::return_;

  using boost::mpl::apply_wrap2;

  typedef apply_wrap2<return_<int1>, int2, int3> acc;
  
  typedef return_<char_x> return_x;
}

BOOST_METAPARSE_TEST_CASE(return)
{
  using boost::metaparse::get_result;
  using boost::metaparse::get_remaining;
  using boost::metaparse::get_position;
  using boost::metaparse::start;
  
  using boost::mpl::equal_to;

  // test_for_non_empty_string
  BOOST_MPL_ASSERT((
    equal_to<get_result<apply_wrap2<return_x, str_hello, start> >::type, char_x>
  ));

  // test_for_empty_string
  BOOST_MPL_ASSERT((
    equal_to<get_result<apply_wrap2<return_x, str_, start> >::type, char_x>
  ));

 
  // test_get_result
  BOOST_MPL_ASSERT((equal_to<int1, get_result<acc>::type>));

  // test_get_remaining
  BOOST_MPL_ASSERT((equal_to<int2, get_remaining<acc>::type>));

  // test_get_position
  BOOST_MPL_ASSERT((equal_to<int3, get_position<acc>::type>));
}


