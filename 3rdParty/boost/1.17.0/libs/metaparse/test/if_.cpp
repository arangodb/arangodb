// Copyright Abel Sinkovics (abel@sinkovics.hu) 2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/if_.hpp>
#include <boost/metaparse/digit.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_result.hpp>

#include "common.hpp"

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(if)
{
  using boost::metaparse::get_result;
  using boost::metaparse::if_;
  using boost::metaparse::digit;
  using boost::metaparse::start;
  
  using boost::mpl::equal_to;
  using boost::mpl::apply_wrap2;

  // test_true
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<apply_wrap2<if_<digit, int11, int13>, str_1, start> >::type,
      int11
    >
  ));
    
  // test_false
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<apply_wrap2<if_<digit, int11, int13>, str_a, start> >::type,
      int13
    >
  ));
}


