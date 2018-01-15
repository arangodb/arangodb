// Copyright Abel Sinkovics (abel@sinkovics.hu) 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/v1/swap.hpp>

#include <boost/type_traits/is_same.hpp>

#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

namespace
{
  struct returns_first
  {
    typedef returns_first type;

    template <class A, class>
    struct apply
    {
      typedef A type;
    };
  };

  struct returns_second
  {
    typedef returns_second type;

    template <class, class B>
    struct apply
    {
      typedef B type;
    };
  };
}

BOOST_METAPARSE_TEST_CASE(swap)
{
  using boost::metaparse::v1::swap;
  using boost::is_same;

  BOOST_MPL_ASSERT((
    is_same<double, swap<returns_first>::apply<int, double>::type>
  ));

  BOOST_MPL_ASSERT((
    is_same<int, swap<returns_second>::apply<int, double>::type>
  ));
}

