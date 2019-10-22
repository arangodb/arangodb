// Copyright Abel Sinkovics (abel@sinkovics.hu) 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/v1/impl/has_type.hpp>

#include <boost/mpl/assert.hpp>
#include <boost/mpl/not.hpp>

#include "test_case.hpp"

namespace
{
  struct nullary_metafunction
  {
    typedef int type;
  };
}

BOOST_METAPARSE_TEST_CASE(has_type)
{
  using boost::metaparse::v1::impl::has_type;
  using boost::mpl::not_;

  // test_int_has_no_type
  BOOST_MPL_ASSERT((not_<has_type<int>::type>));

  // test_nullary_metafunction_has_type
  BOOST_MPL_ASSERT((has_type<nullary_metafunction>));
}

