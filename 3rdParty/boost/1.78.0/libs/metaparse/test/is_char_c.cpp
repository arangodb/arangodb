// Copyright Abel Sinkovics (abel@sinkovics.hu) 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/v1/impl/is_char_c.hpp>

#include <boost/mpl/assert.hpp>
#include <boost/mpl/char.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(is_char_c)
{
  using boost::metaparse::v1::impl::is_char_c;

  using boost::mpl::char_;

  // test_for_the_same_char
  BOOST_MPL_ASSERT(( is_char_c<'x'>::apply<char_<'x'> > ));

  // test_for_another_char
  BOOST_MPL_ASSERT_NOT(( is_char_c<'x'>::apply<char_<'y'> > ));
}

