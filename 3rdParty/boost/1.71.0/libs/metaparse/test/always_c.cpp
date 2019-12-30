// Copyright Abel Sinkovics (abel@sinkovics.hu) 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/always_c.hpp>
#include <boost/metaparse/is_error.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_result.hpp>

#include "common.hpp"

#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(always_c)
{
  using boost::metaparse::get_result;
  using boost::metaparse::always_c;
  using boost::metaparse::start;
  using boost::metaparse::is_error;
  
  using boost::mpl::equal_to;
  using boost::mpl::apply_wrap2;

  // test_result
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<apply_wrap2<always_c<'1', int13>, str_1, start> >::type,
      int13
    >
  ));
  
  // test_fail
  BOOST_MPL_ASSERT((
    is_error<apply_wrap2<always_c<'1', int13>, str_a, start> >
  ));
}

