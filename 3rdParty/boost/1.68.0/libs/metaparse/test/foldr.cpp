// Copyright Abel Sinkovics (abel@sinkovics.hu) 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/metaparse/foldr.hpp>
#include <boost/metaparse/v1/impl/front_inserter.hpp>

#include <boost/mpl/list.hpp>

using boost::metaparse::foldr;
using boost::metaparse::v1::impl::front_inserter;

using boost::mpl::list;

namespace
{
  template <class P>
  struct repeated : foldr<P, list<>, front_inserter> {};
}

#define TEST_NAME foldr

#include "repeated_test.hpp"

