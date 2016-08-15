// Copyright Abel Sinkovics (abel@sinkovics.hu) 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/v1/impl/size.hpp>
#include <boost/metaparse/string.hpp>

#include <boost/mpl/equal_to.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/assert.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(size)
{
  using boost::metaparse::v1::impl::size;
  using boost::metaparse::string;
  
  using boost::mpl::equal_to;
  using boost::mpl::int_;

  // test_0
  BOOST_MPL_ASSERT((equal_to<int_<0>, size<string<> >::type>));

  // test_1_
  BOOST_MPL_ASSERT((equal_to<int_<1>, size<string<'x'> >::type>));

  // test_5
  BOOST_MPL_ASSERT((equal_to<int_<4>, size<string<'1','2','3','4'> >::type>));

}


