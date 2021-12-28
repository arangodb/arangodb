//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2013-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//[doc_custom_small_vector
#include <boost/container/small_vector.hpp>
#include <boost/static_assert.hpp>

//Make sure assertions are active
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main ()
{
   using namespace boost::container;

   //This option specifies the desired alignment for the internal value_type
   typedef small_vector_options< inplace_alignment<16u> >::type alignment_16_option_t;

   //Check 16 byte alignment option
   small_vector<int, 10, void, alignment_16_option_t > sv;
   assert(((std::size_t)sv.data() % 16u) == 0);


   //This option specifies that a vector will increase its capacity 50%
   //each time the previous capacity was exhausted.
   typedef small_vector_options< growth_factor<growth_factor_50> >::type growth_50_option_t;

   //Fill the vector until full capacity is reached
   small_vector<int, 10, void, growth_50_option_t > growth_50_vector(10, 0);
   const std::size_t old_cap = growth_50_vector.capacity();
   growth_50_vector.resize(old_cap);

   //Now insert an additional item and check the new buffer is 50% bigger
   growth_50_vector.push_back(1);
   assert(growth_50_vector.capacity() == old_cap*3/2);

   return 0;
}
//]
