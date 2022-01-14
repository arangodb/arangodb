//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/map.hpp>

struct empty
{
   friend bool operator == (const empty &, const empty &){ return true; }
   friend bool operator <  (const empty &, const empty &){ return true; }
};

template class ::boost::container::map<empty, empty>;
template class ::boost::container::multimap<empty, empty>;

volatile ::boost::container::map<empty, empty> dummy;
volatile ::boost::container::multimap<empty, empty> dummy2;

#include <boost/container/allocator.hpp>
#include <boost/container/adaptive_pool.hpp>
#include "movable_int.hpp"
#include "dummy_test_allocator.hpp"

namespace boost {
namespace container {

typedef std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int> pair_t;

//Explicit instantiation to detect compilation errors

//map
template class map
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , test::simple_allocator< pair_t >
   >;

template class map
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , adaptive_pool< pair_t >
   >;
}} //boost::container

int main()
{
   return 0;
}
