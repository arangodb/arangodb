// Copyright Abel Sinkovics (abel@sinkovics.hu) 2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/except.hpp>
#include <boost/metaparse/one_char.hpp>
#include <boost/metaparse/fail.hpp>
#include <boost/metaparse/is_error.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/get_result.hpp>

#include "common.hpp"

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/apply_wrap.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(except)
{
  using boost::metaparse::is_error;
  using boost::metaparse::except;
  using boost::metaparse::one_char;
  using boost::metaparse::start;
  using boost::metaparse::get_result;
  using boost::metaparse::fail;
  
  using boost::mpl::apply_wrap2;
  using boost::mpl::equal_to;

  // test_with_good
  BOOST_MPL_ASSERT((
    is_error<
      apply_wrap2<except<one_char, int13, test_failure>, str_hello, start>
    >
  ));
  
  // test_with_bad
  BOOST_MPL_ASSERT((
    equal_to<
      get_result<
        apply_wrap2<
          except<fail<test_failure>, int13, test_failure>,
          str_hello,
          start
        >
      >::type,
      int13
    >
  ));
}



