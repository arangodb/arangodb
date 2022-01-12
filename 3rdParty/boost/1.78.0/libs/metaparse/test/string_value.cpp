// Copyright Abel Sinkovics (abel@sinkovics.hu) 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE string_value

#include <boost/metaparse/string_value.hpp>
#include <boost/metaparse/string.hpp>

#include <boost/test/unit_test.hpp>

#if BOOST_METAPARSE_STD >= 2011
#include <type_traits>
#endif

BOOST_AUTO_TEST_CASE(test_string_value)
{
#if BOOST_METAPARSE_STD >= 2011
  auto foo = BOOST_METAPARSE_STRING_VALUE("foo");

  BOOST_MPL_ASSERT((
    std::is_same<BOOST_METAPARSE_STRING("foo"), decltype(foo)>
  ));
#endif
}

