// Copyright Abel Sinkovics (abel@sinkovics.hu) 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE empty_string

#include <boost/metaparse/config.hpp>
#if BOOST_METAPARSE_STD >= 2011
#  include <boost/metaparse/v1/cpp11/impl/empty_string.hpp>
#else
#  include <boost/metaparse/v1/cpp98/impl/empty_string.hpp>
#endif

#include <boost/test/unit_test.hpp>

#include <string>

BOOST_AUTO_TEST_CASE(test_empty_string)
{
  using boost::metaparse::v1::impl::empty_string;
  using std::string;
  
  // test_value
  BOOST_REQUIRE_EQUAL(string(), empty_string<>::value);
}

