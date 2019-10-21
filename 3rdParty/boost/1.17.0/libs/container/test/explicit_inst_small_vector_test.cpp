//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/small_vector.hpp>

struct empty
{
   friend bool operator == (const empty &, const empty &){ return true; }
   friend bool operator <  (const empty &, const empty &){ return true; }
};

template class ::boost::container::small_vector<empty, 2>;
volatile ::boost::container::small_vector<empty, 2> dummy;

#include <boost/container/allocator.hpp>
#include "movable_int.hpp"
#include "dummy_test_allocator.hpp"

namespace boost {
namespace container {

template class small_vector<char, 0>;
template class small_vector<char, 1>;
template class small_vector<char, 2>;
template class small_vector<char, 10>;

template class small_vector<int, 0>;
template class small_vector<int, 1>;
template class small_vector<int, 2>;
template class small_vector<int, 10>;

//Explicit instantiation to detect compilation errors
template class boost::container::small_vector
   < test::movable_and_copyable_int
   , 10
   , test::simple_allocator<test::movable_and_copyable_int> >;

template class boost::container::small_vector
   < test::movable_and_copyable_int
   , 10
   , allocator<test::movable_and_copyable_int> >;

}}


int main()
{
   return 0;
}
