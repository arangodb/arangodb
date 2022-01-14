// Copyright Abel Sinkovics (abel@sinkovics.hu) 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/fail.hpp>
#include <boost/metaparse/get_message.hpp>
#include <boost/metaparse/get_position.hpp>
#include <boost/metaparse/start.hpp>

#include "common.hpp"

#include <boost/type_traits/is_same.hpp>

#include <boost/mpl/assert.hpp>
#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/apply_wrap.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(fail_tag)
{
  using boost::metaparse::fail;
  using boost::metaparse::get_message;
  using boost::metaparse::get_position;
  using boost::metaparse::start;
  
  using boost::mpl::apply_wrap2;
  using boost::mpl::equal_to;
  using boost::is_same;

  typedef fail<test_failure> p;
  typedef apply_wrap2<p, str_hello, start> failed_result;

  // test_getting_message_back
  BOOST_MPL_ASSERT((is_same<test_failure, get_message<failed_result>::type>));

  // test_getting_position
  BOOST_MPL_ASSERT((equal_to<start, get_position<failed_result>::type>));
}

