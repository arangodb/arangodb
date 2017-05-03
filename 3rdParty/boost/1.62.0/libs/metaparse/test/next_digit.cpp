// Copyright Abel Sinkovics (abel@sinkovics.hu) 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/v1/impl/next_digit.hpp>

#include <boost/mpl/assert.hpp>
#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/int.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(next_digit)
{
  using boost::metaparse::v1::impl::next_digit;

  using boost::mpl::equal_to;
  using boost::mpl::int_;

  BOOST_MPL_ASSERT((equal_to<int_< 0>, next_digit::apply<int_<0>, int_<0> > >));
  BOOST_MPL_ASSERT((equal_to<int_<10>, next_digit::apply<int_<1>, int_<0> > >));
  BOOST_MPL_ASSERT((equal_to<int_<13>, next_digit::apply<int_<1>, int_<3> > >));
}

