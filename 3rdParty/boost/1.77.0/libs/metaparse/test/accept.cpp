// Copyright Abel Sinkovics (abel@sinkovics.hu) 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/accept.hpp>
#include <boost/metaparse/start.hpp>
#include <boost/metaparse/string.hpp>
#include <boost/metaparse/get_result.hpp>
#include <boost/metaparse/get_position.hpp>
#include <boost/metaparse/get_remaining.hpp>

#include <boost/mpl/assert.hpp>

#include <boost/type_traits.hpp>

#include "test_case.hpp"

namespace
{
  template <class T>
  struct returns
  {
    typedef T type;
  };

  template <class T>
  struct gets_foo
  {
    typedef typename T::foo type;
  };
}

BOOST_METAPARSE_TEST_CASE(accept)
{
  using boost::metaparse::accept;
  using boost::metaparse::start;
  using boost::metaparse::string;
  using boost::metaparse::get_result;
  using boost::metaparse::get_position;
  using boost::metaparse::get_remaining;

  using boost::is_same;

  typedef string<'H','e','l','l','o'> s;

  // test_accept_is_metaprogramming_value
  BOOST_MPL_ASSERT((
    is_same<accept<int, s, start>, accept<int, s, start>::type>
  ));

  // test_accept_is_not_lazy
  BOOST_MPL_ASSERT((
    is_same<
      accept<gets_foo<int>, s, start>,
      accept<gets_foo<int>, returns<s>, returns<start> >::type
    >
  ));

  // test_get_result_of_accept
  BOOST_MPL_ASSERT((is_same<int, get_result<accept<int, s, start> >::type>));

  // test_get_remaining_of_accept
  BOOST_MPL_ASSERT((is_same<s, get_remaining<accept<int, s, start> >::type>));

  // test_get_position_of_accept
  BOOST_MPL_ASSERT((
    is_same<start, get_position<accept<int, s, start> >::type>
  ));
}

