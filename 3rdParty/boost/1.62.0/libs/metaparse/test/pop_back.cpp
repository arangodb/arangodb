// Copyright Abel Sinkovics (abel@sinkovics.hu) 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/v1/impl/pop_back.hpp>
#include <boost/metaparse/string.hpp>

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/char.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(pop_back)
{
  using boost::metaparse::v1::impl::pop_back;
  using boost::metaparse::string;
  
  using boost::mpl::equal_to;
  using boost::mpl::char_;

  typedef string<'h','e','l','l','o'> hello;

  // test_pop_back
  BOOST_MPL_ASSERT((equal_to<string<'h','e','l','l'>, pop_back<hello>::type>));

  // test_pop_back_one_element
  BOOST_MPL_ASSERT((equal_to<string<>, pop_back<string<'x'> >::type>));
}


