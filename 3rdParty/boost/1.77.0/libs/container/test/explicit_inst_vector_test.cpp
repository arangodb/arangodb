//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/vector.hpp>

struct empty
{
   friend bool operator == (const empty &, const empty &){ return true; }
   friend bool operator <  (const empty &, const empty &){ return true; }
};

template class ::boost::container::vector<empty>;
volatile ::boost::container::vector<empty> dummy;

#include <boost/container/allocator.hpp>
#include "movable_int.hpp"
#include "dummy_test_allocator.hpp"

class CustomAllocator
{
   public:
	typedef int value_type;
	typedef value_type* pointer;
	typedef const value_type* const_pointer;
	typedef unsigned short size_type;
	typedef short difference_type;

	pointer allocate(size_type count)
   {  return (pointer)new char[sizeof(value_type)*count]; }

	void deallocate(pointer ptr, size_type )
   {  delete [](char*)ptr; }

   friend bool operator==(CustomAllocator const&, CustomAllocator const&) BOOST_NOEXCEPT
   {  return true;   }

   friend bool operator!=(CustomAllocator const& x, CustomAllocator const& y) BOOST_NOEXCEPT
   {  return !(x == y);  }
};


namespace boost {
namespace container {

//Explicit instantiation to detect compilation errors
template class boost::container::vector
   < test::movable_and_copyable_int
   , test::simple_allocator<test::movable_and_copyable_int> >;

template class boost::container::vector
   < test::movable_and_copyable_int
   , allocator<test::movable_and_copyable_int> >;

template class vec_iterator<int*, true >;
template class vec_iterator<int*, false>;

//Test stored_size option
template class boost::container::vector< test::movable_and_copyable_int
                                       , new_allocator<test::movable_and_copyable_int>
                                       , vector_options< stored_size<unsigned short> >::type
                                       >;

//test custom allocator with small size_type
template class boost::container::vector<int, CustomAllocator>;

}  //namespace boost {
}  //namespace container {

int main()
{
   return 0;
}
