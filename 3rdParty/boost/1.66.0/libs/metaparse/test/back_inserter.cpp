// Copyright Abel Sinkovics (abel@sinkovics.hu) 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/v1/impl/back_inserter.hpp>

#include <boost/mpl/assert.hpp>
#include <boost/mpl/equal.hpp>
#include <boost/mpl/deque.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(back_inserter)
{
  using boost::metaparse::v1::impl::back_inserter;

  using boost::mpl::equal;
  using boost::mpl::deque;

  // test_inserts_at_the_back
  BOOST_MPL_ASSERT((
    equal<deque<int, char>, back_inserter::apply<deque<int>, char>::type>
  ));
}

