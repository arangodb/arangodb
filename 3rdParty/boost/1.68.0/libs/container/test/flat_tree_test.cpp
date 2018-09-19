//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/detail/flat_tree.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/stable_vector.hpp>
#include <boost/container/static_vector.hpp>

#include "movable_int.hpp"
#include "dummy_test_allocator.hpp"

using namespace boost::container;

typedef boost::container::dtl::pair<test::movable_and_copyable_int, test::movable_and_copyable_int> pair_t;

namespace boost {
namespace container {

//Explicit instantiation to detect compilation errors

namespace dtl {

template class flat_tree
   < pair_t
   , select1st<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , test::simple_allocator<pair_t>
   >;

template class flat_tree
   < pair_t
   , select1st<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , std::allocator<pair_t>
   >;

template class flat_tree
   < pair_t
   , select1st<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , small_vector<pair_t, 10>
   >;

template class flat_tree
   < pair_t
   , select1st<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , stable_vector<pair_t>
   >;

template class flat_tree
   < test::movable_and_copyable_int
   , identity<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , test::simple_allocator<test::movable_and_copyable_int>
   >;

template class flat_tree
   < test::movable_and_copyable_int
   , identity<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , std::allocator<test::movable_and_copyable_int>
   >;

template class flat_tree
   < test::movable_and_copyable_int
   , identity<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , small_vector<test::movable_and_copyable_int, 10>
   >;

template class flat_tree
   < test::movable_and_copyable_int
   , identity<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , stable_vector<test::movable_and_copyable_int>
   >;

template class flat_tree
< test::movable_and_copyable_int
   , identity<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , static_vector<test::movable_and_copyable_int, 10>
>;

}  //dtl {
}} //boost::container

#if (__cplusplus > 201103L)
#include <vector>

namespace boost{
namespace container{
namespace dtl{

template class flat_tree
< test::movable_and_copyable_int
   , identity<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , std::vector<test::movable_and_copyable_int>
>;

template class flat_tree
< pair_t
   , select1st<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , std::vector<pair_t>
>;

}  //dtl {
}} //boost::container

#endif

int main ()
{
   return 0;
}
