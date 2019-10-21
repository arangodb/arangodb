// Copyright Abel Sinkovics (abel@sinkovics.hu) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_METAPARSE_LIMIT_STRING_SIZE 4

#include <boost/type_traits/is_same.hpp>

#include "common.hpp"
#include "test_case.hpp"
#include "string_macros.hpp"

#include <boost/metaparse/string.hpp>

#ifndef BOOST_METAPARSE_V1_CONFIG_NO_BOOST_METAPARSE_STRING

using boost::metaparse::string;
using boost::is_same;

BOOST_METAPARSE_TEST_CASE(string_macro)
{
  BOOST_MPL_ASSERT((
    is_same<string<'a', 'b', 'c', 'd'>, BOOST_METAPARSE_STRING("abcd")>
  ));
}

#undef BOOST_METAPARSE_LIMIT_STRING_SIZE
#define BOOST_METAPARSE_LIMIT_STRING_SIZE 8

BOOST_METAPARSE_TEST_CASE(string_macro_redefined_length_limit)
{
  BOOST_MPL_ASSERT((
    is_same<string<'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'>,
    BOOST_METAPARSE_STRING("abcdefgh")>
  ));
}

#undef BOOST_METAPARSE_LIMIT_STRING_SIZE
#define BOOST_METAPARSE_LIMIT_STRING_SIZE 127

BOOST_METAPARSE_TEST_CASE(creating_long_string_with_macro)
{
  BOOST_MPL_ASSERT((
    is_same<
      string<
        BOOST_METAPARSE_TEST_CHARS_100,
        BOOST_METAPARSE_TEST_CHARS_10,
        BOOST_METAPARSE_TEST_CHARS_10,
        '0', '1', '2', '3', '4', '5', '6'
      >,
      BOOST_METAPARSE_STRING(
        BOOST_METAPARSE_TEST_STRING_100
        BOOST_METAPARSE_TEST_STRING_10
        BOOST_METAPARSE_TEST_STRING_10
        "0123456"
      )
    >
  ));
}

#endif

