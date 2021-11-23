// Copyright Abel Sinkovics (abel@sinkovics.hu) 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE define_error

#include <boost/metaparse/define_error.hpp>

#include <boost/type_traits/is_same.hpp>
#include <boost/mpl/assert.hpp>

#include <boost/test/unit_test.hpp>

namespace
{
  BOOST_METAPARSE_DEFINE_ERROR(test_error, "test error");
}

BOOST_AUTO_TEST_CASE(test_define_error)
{
  using boost::is_same;

  BOOST_MPL_ASSERT((is_same<test_error, test_error::type>));

  BOOST_CHECK_EQUAL("test error", test_error::get_value());
}

