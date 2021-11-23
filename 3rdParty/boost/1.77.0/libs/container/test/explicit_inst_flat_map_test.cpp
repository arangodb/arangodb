//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/flat_map.hpp>
#include <boost/container/allocator.hpp>
#include "movable_int.hpp"
#include "dummy_test_allocator.hpp"
#include <boost/container/stable_vector.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/deque.hpp>
#include <boost/container/static_vector.hpp>

struct empty
{
   friend bool operator == (const empty &, const empty &){ return true; }
   friend bool operator <  (const empty &, const empty &){ return true; }
};

template class ::boost::container::flat_map<empty, empty>;
template class ::boost::container::flat_multimap<empty, empty>;

volatile ::boost::container::flat_map<empty, empty> dummy;
volatile ::boost::container::flat_multimap<empty, empty> dummy2;

namespace boost {
namespace container {

//Explicit instantiation to detect compilation errors

//flat_map
typedef std::pair<test::movable_and_copyable_int, test::movable_and_copyable_int> test_pair_t;

template class flat_map
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , small_vector< test_pair_t, 10, std::allocator< test_pair_t > >
   >;

//flat_multimap
template class flat_multimap
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , stable_vector< test_pair_t, allocator< test_pair_t > >
   >;

template class flat_multimap
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , deque<test_pair_t, test::simple_allocator< test_pair_t > >
   >;

template class flat_multimap
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , static_vector<test_pair_t, 10 >
   >;

//As flat container iterators are typedefs for vector::[const_]iterator,
//no need to explicit instantiate them

}} //boost::container

#if (__cplusplus > 201103L)
#include <vector>

namespace boost{
namespace container{

template class flat_map
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , std::vector<test_pair_t>
>;

}} //boost::container

#endif

int main()
{
   return 0;
}
