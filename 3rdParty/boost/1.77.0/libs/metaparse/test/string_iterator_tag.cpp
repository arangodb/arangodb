// Copyright Abel Sinkovics (abel@sinkovics.hu) 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/v1/impl/string_iterator_tag.hpp>

#include <boost/mpl/assert.hpp>

#include <boost/type_traits/is_same.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(string_iterator_tag)
{
  using boost::metaparse::v1::impl::string_iterator_tag;
  
  using boost::is_same;

  // test_metaprogramming_value
  BOOST_MPL_ASSERT((is_same<string_iterator_tag, string_iterator_tag::type>));
}

