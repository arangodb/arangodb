//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/small_vector.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/assert.hpp>
using namespace boost::container;

const std::size_t Capacity = 10u;

void test_alignment()
{
   {  //extended alignment
      const std::size_t extended_alignment = sizeof(int)*4u;
      BOOST_STATIC_ASSERT(extended_alignment > dtl::alignment_of<int>::value);
      #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
      using options_t = small_vector_options_t< inplace_alignment<extended_alignment> >;
      #else
      typedef small_vector_options
         < inplace_alignment<extended_alignment> >::type options_t;
      #endif

      small_vector<int, Capacity, void, options_t> v;
      v.resize(v.capacity());
      BOOST_ASSERT((reinterpret_cast<std::size_t>(&v[0]) % extended_alignment) == 0);
   }
   {  //default alignment
      #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
      using options_t = small_vector_options_t< inplace_alignment<0> >;
      #else
      typedef small_vector_options< inplace_alignment<0> >::type options_t;
      #endif

      small_vector<int, Capacity, void, options_t> v;
      v.resize(v.capacity());
      BOOST_ASSERT((reinterpret_cast<std::size_t>(&v[0]) % dtl::alignment_of<int>::value) == 0);
   }
}

void test_growth_factor_50()
{
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
   using options_t = small_vector_options_t< growth_factor<growth_factor_50> >;
   #else
   typedef small_vector_options
      < growth_factor<growth_factor_50> >::type options_t;
   #endif

   small_vector<int, Capacity, new_allocator<int>, options_t> v;

   v.resize(5);
   v.resize(v.capacity());
   std::size_t old_capacity = v.capacity();
   v.push_back(0);
   std::size_t new_capacity = v.capacity();
   BOOST_TEST(new_capacity == old_capacity + old_capacity/2);
}

void test_growth_factor_60()
{
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
   using options_t = small_vector_options_t< growth_factor<growth_factor_60> >;
   #else
   typedef small_vector_options
      < growth_factor<growth_factor_60> >::type options_t;
   #endif

   small_vector<int, Capacity, new_allocator<int>, options_t> v;

   v.resize(5);
   v.resize(v.capacity());
   std::size_t old_capacity = v.capacity();
   v.push_back(0);
   std::size_t new_capacity = v.capacity();
   BOOST_TEST(new_capacity == old_capacity + 3*old_capacity/5);
}

void test_growth_factor_100()
{
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
   using options_t = small_vector_options_t< growth_factor<growth_factor_100> >;
   #else
   typedef small_vector_options
      < growth_factor<growth_factor_100> >::type options_t;
   #endif

   small_vector<int, Capacity, new_allocator<int>, options_t> v;

   v.resize(5);
   v.resize(v.capacity());
   std::size_t old_capacity = v.capacity();
   v.push_back(0);
   std::size_t new_capacity = v.capacity();
   BOOST_TEST(new_capacity == 2*old_capacity);
}

int main()
{
   test_alignment();
   test_growth_factor_50();
   test_growth_factor_60();
   test_growth_factor_100();
   return ::boost::report_errors();
}
