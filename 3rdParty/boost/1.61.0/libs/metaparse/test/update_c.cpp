// Copyright Abel Sinkovics (abel@sinkovics.hu) 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/v1/impl/update_c.hpp>
#include <boost/metaparse/string.hpp>

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(update_c)
{
  using boost::metaparse::v1::impl::update_c;
  using boost::metaparse::string;
  
  using boost::mpl::equal_to;

  typedef string<'h','e','l','l','o'> hello;

  // test_update_first_char
  BOOST_MPL_ASSERT((
    equal_to<string<'x','e','l','l','o'>, update_c<hello, 0, 'x'>::type>
  ));

  // test_update_middle_char
  BOOST_MPL_ASSERT((
    equal_to<string<'h','e','x','l','o'>, update_c<hello, 2, 'x'>::type>
  ));

  // test_update_last_char
  BOOST_MPL_ASSERT((
    equal_to<string<'h','e','l','l','x'>, update_c<hello, 4, 'x'>::type>
  ));
}

