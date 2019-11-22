//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/slist.hpp>

struct empty
{
   friend bool operator == (const empty &, const empty &){ return true; }
   friend bool operator <  (const empty &, const empty &){ return true; }
};

template class ::boost::container::slist<empty>;
volatile ::boost::container::slist<empty> dummy;

#include <boost/container/allocator.hpp>
#include <boost/container/node_allocator.hpp>
#include "movable_int.hpp"
#include "dummy_test_allocator.hpp"

namespace boost {
namespace container {

//Explicit instantiation to detect compilation errors
template class boost::container::slist
   < test::movable_and_copyable_int
   , test::simple_allocator<test::movable_and_copyable_int> >;

template class boost::container::slist
   < test::movable_and_copyable_int
   , node_allocator<test::movable_and_copyable_int> >;

}}

int main()
{
   return 0;
}
