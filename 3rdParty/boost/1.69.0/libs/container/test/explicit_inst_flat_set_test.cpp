//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/flat_set.hpp>

struct empty
{
   friend bool operator == (const empty &, const empty &){ return true; }
   friend bool operator <  (const empty &, const empty &){ return true; }
};

template class ::boost::container::flat_set<empty>;
template class ::boost::container::flat_multiset<empty>;

volatile ::boost::container::flat_set<empty> dummy;
volatile ::boost::container::flat_multiset<empty> dummy2;

#include <boost/container/allocator.hpp>
#include "movable_int.hpp"
#include "dummy_test_allocator.hpp"
#include <boost/container/stable_vector.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/deque.hpp>
#include <boost/container/static_vector.hpp>

namespace boost {
namespace container {

//Explicit instantiation to detect compilation errors

//flat_set
template class flat_set
   < test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , small_vector<test::movable_and_copyable_int, 10, allocator<test::movable_and_copyable_int> >
   >;

//flat_multiset
template class flat_multiset
   < test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , stable_vector<test::movable_and_copyable_int, test::simple_allocator<test::movable_and_copyable_int> >
   >;

template class flat_multiset
   < test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , deque<test::movable_and_copyable_int, test::simple_allocator< test::movable_and_copyable_int > >
   >;

template class flat_multiset
   < test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , static_vector<test::movable_and_copyable_int, 10 >
   >;

//As flat container iterators are typedefs for vector::[const_]iterator,
//no need to explicit instantiate them

}} //boost::container

#if (__cplusplus > 201103L)
#include <vector>

namespace boost{
namespace container{

template class flat_set
   < test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , std::vector<test::movable_and_copyable_int>
>;

}} //boost::container

#endif

int main()
{
   return 0;
}
