// Copyright Abel Sinkovics (abel@sinkovics.hu) 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/v1/impl/concat.hpp>
#include <boost/metaparse/string.hpp>

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/char.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(concat)
{
  using boost::metaparse::v1::impl::concat;
  using boost::metaparse::string;
  
  using boost::mpl::equal_to;

  typedef string<> empty;
  typedef string<'h','e','l','l','o'> hello;

  // test_empty_empty
  BOOST_MPL_ASSERT((equal_to<empty, concat<empty, empty>::type>));

  // test_empty_hello
  BOOST_MPL_ASSERT((equal_to<hello, concat<empty, hello>::type>));

  // test_hello_empty
  BOOST_MPL_ASSERT((equal_to<hello, concat<hello, empty>::type>));

  // test_hello_hello
  BOOST_MPL_ASSERT((
    equal_to<
      string<'h','e','l','l','o','h','e','l','l','o'>,
      concat<hello, hello>::type
    >
  ));
}


