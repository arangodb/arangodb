//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/detail/tree.hpp>
#include <boost/container/adaptive_pool.hpp>

#include "movable_int.hpp"
#include "dummy_test_allocator.hpp"

using namespace boost::container;

typedef std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int> pair_t;

namespace boost {
namespace container {

//Explicit instantiation to detect compilation errors

namespace dtl {

//Instantiate base class as previous instantiations don't instantiate inherited members
template class tree
   < pair_t
   , select1st<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , test::simple_allocator<pair_t>
   , tree_assoc_defaults
   >;

template class tree
   < pair_t
   , select1st<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , std::allocator<pair_t>
   , tree_assoc_defaults
   >;

template class tree
   < pair_t
   , select1st<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , adaptive_pool<pair_t>
   , tree_assoc_defaults
   >;

template class tree
   < test::movable_and_copyable_int
   , identity<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , test::simple_allocator<test::movable_and_copyable_int>
   , tree_assoc_defaults
   >;

template class tree
   < test::movable_and_copyable_int
   , identity<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , std::allocator<test::movable_and_copyable_int>
   , tree_assoc_defaults
   >;

template class tree
   < test::movable_and_copyable_int
   , identity<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , adaptive_pool<test::movable_and_copyable_int>
   , tree_assoc_defaults
   >;

}  //dtl {

}} //boost::container

int main ()
{
   return 0;
}
