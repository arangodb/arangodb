// Copyright Abel Sinkovics (abel@sinkovics.hu) 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/foldl_reject_incomplete.hpp>
#include <boost/metaparse/v1/impl/back_inserter.hpp>

#include <boost/mpl/vector.hpp>

using boost::metaparse::foldl_reject_incomplete;
using boost::metaparse::v1::impl::back_inserter;

using boost::mpl::vector;

namespace
{
  template <class P>
  struct repeated_reject_incomplete :
    foldl_reject_incomplete<P, vector<>, back_inserter>
  {};
}

#define TEST_NAME foldl_reject_incomplete

#include "repeated_reject_incomplete_test.hpp"

