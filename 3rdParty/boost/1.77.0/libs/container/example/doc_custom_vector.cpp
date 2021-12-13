//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2013-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/core/no_exceptions_support.hpp>
//[doc_custom_vector
#include <boost/container/vector.hpp>
#include <boost/static_assert.hpp>
#include <boost/core/no_exceptions_support.hpp>

//Make sure assertions are active
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main ()
{
   using namespace boost::container;

   //This option specifies that a vector that will use "unsigned char" as
   //the type to store capacity or size internally.
   typedef vector_options< stored_size<unsigned char> >::type size_option_t;

   //Size-optimized vector is smaller than the default one.
   typedef vector<int, new_allocator<int>, size_option_t > size_optimized_vector_t;
   BOOST_STATIC_ASSERT(( sizeof(size_optimized_vector_t) < sizeof(vector<int>) ));

   //Requesting capacity for more elements than representable by "unsigned char"
   //is an error in the size optimized vector.
   bool exception_thrown = false;
   /*<-*/ 
   #ifndef BOOST_NO_EXCEPTIONS
   BOOST_TRY{ size_optimized_vector_t v(256); } BOOST_CATCH(...){ exception_thrown = true; } BOOST_CATCH_END
   #else
   exception_thrown = true;
   #endif   //BOOST_NO_EXCEPTIONS
   /*->*/
   //=try       { size_optimized_vector_t v(256); }
   //=catch(...){ exception_thrown = true;        }

   assert(exception_thrown == true);

   //This option specifies that a vector will increase its capacity 50%
   //each time the previous capacity was exhausted.
   typedef vector_options< growth_factor<growth_factor_50> >::type growth_50_option_t;

   //Fill the vector until full capacity is reached
   vector<int, new_allocator<int>, growth_50_option_t > growth_50_vector(5, 0);
   const std::size_t old_cap = growth_50_vector.capacity();
   growth_50_vector.resize(old_cap);

   //Now insert an additional item and check the new buffer is 50% bigger
   growth_50_vector.push_back(1);
   assert(growth_50_vector.capacity() == old_cap*3/2);

   return 0;
}
//]
