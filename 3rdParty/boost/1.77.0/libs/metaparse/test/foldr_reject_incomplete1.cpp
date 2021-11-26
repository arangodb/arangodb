// Copyright Abel Sinkovics (abel@sinkovics.hu) 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/foldr_reject_incomplete1.hpp>
#include <boost/metaparse/v1/impl/front_inserter.hpp>

#include <boost/mpl/list.hpp>

using boost::metaparse::foldr_reject_incomplete1;
using boost::metaparse::v1::impl::front_inserter;

using boost::mpl::list;

namespace
{
  template <class P>
  struct repeated_reject_incomplete1 :
    foldr_reject_incomplete1<P, list<>, front_inserter>
  {};
}

#define TEST_NAME foldr_reject_incomplete1

#include "repeated_reject_incomplete1_test.hpp"

