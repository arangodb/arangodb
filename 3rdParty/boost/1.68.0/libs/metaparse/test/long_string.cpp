// Copyright Abel Sinkovics (abel@sinkovics.hu) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/config.hpp>

#if BOOST_METAPARSE_STD >= 2011

#define BOOST_METAPARSE_LIMIT_STRING_SIZE 1024
#include <boost/metaparse/string.hpp>

#include <boost/type_traits/is_same.hpp>

#include "common.hpp"
#include "test_case.hpp"
#include "string_macros.hpp"

#ifndef BOOST_METAPARSE_V1_CONFIG_NO_BOOST_METAPARSE_STRING

using boost::metaparse::string;
using boost::is_same;

BOOST_METAPARSE_TEST_CASE(creating_very_long_string)
{
// The Oracle Studio limit is 127 characters
#ifndef __SUNPRO_CC
  BOOST_MPL_ASSERT((
    is_same<
      string<
        BOOST_METAPARSE_TEST_CHARS_100,
        BOOST_METAPARSE_TEST_CHARS_100,
        BOOST_METAPARSE_TEST_CHARS_100,
        BOOST_METAPARSE_TEST_CHARS_100,
        BOOST_METAPARSE_TEST_CHARS_100,
        BOOST_METAPARSE_TEST_CHARS_10,
        '0', '1'
      >,
      BOOST_METAPARSE_STRING(
        BOOST_METAPARSE_TEST_STRING_100
        BOOST_METAPARSE_TEST_STRING_100
        BOOST_METAPARSE_TEST_STRING_100
        BOOST_METAPARSE_TEST_STRING_100
        BOOST_METAPARSE_TEST_STRING_100
        BOOST_METAPARSE_TEST_STRING_10
        "01"
      )
    >
  ));
#endif
}

#endif

#endif

