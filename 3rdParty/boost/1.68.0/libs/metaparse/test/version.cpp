// Copyright Abel Sinkovics (abel@sinkovics.hu) 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/version.hpp>

#include <boost/mpl/assert.hpp>
#include <boost/mpl/bool.hpp>

#include "test_case.hpp"

BOOST_METAPARSE_TEST_CASE(version)
{
  using boost::mpl::bool_;

  // test_version_number
  BOOST_MPL_ASSERT((bool_<BOOST_METAPARSE_VERSION == 10000000>));
}

