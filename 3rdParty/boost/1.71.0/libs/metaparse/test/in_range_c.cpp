// Copyright Abel Sinkovics (abel@sinkovics.hu) 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/util/in_range_c.hpp>

#include "common.hpp"

#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/not.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(util_in_range_c)
{
  using boost::metaparse::util::in_range_c;
  
  using boost::mpl::apply_wrap1;
  using boost::mpl::not_;

  // test_int_in_range
  BOOST_MPL_ASSERT((apply_wrap1<in_range_c<char, 'a', 'g'>, char_e>));

  // test_lower_bound
  BOOST_MPL_ASSERT((apply_wrap1<in_range_c<char, 'a', 'g'>, char_a>));

  // test_upper_bound
  BOOST_MPL_ASSERT((apply_wrap1<in_range_c<int, 10, 13>, int13>));

  // test_int_not_in_range
  BOOST_MPL_ASSERT((not_<apply_wrap1<in_range_c<int, 10, 13>, int14> >));
  
}

