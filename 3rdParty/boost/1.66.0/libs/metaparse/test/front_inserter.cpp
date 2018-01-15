// Copyright Abel Sinkovics (abel@sinkovics.hu) 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/v1/impl/front_inserter.hpp>

#include <boost/mpl/assert.hpp>
#include <boost/mpl/equal.hpp>
#include <boost/mpl/deque.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(front_inserter)
{
  using boost::metaparse::v1::impl::front_inserter;

  using boost::mpl::equal;
  using boost::mpl::deque;

  // test_inserts_at_the_front
  BOOST_MPL_ASSERT((
    equal<deque<int, char>, front_inserter::apply<deque<char>, int>::type>
  ));
}

