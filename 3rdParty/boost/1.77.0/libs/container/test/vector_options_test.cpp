//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/vector.hpp>
#include <boost/container/allocator.hpp>
#include <boost/container/detail/next_capacity.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::container;

template<class Unsigned, class VectorType>
void test_stored_size_type_impl()
{
   #ifndef BOOST_NO_EXCEPTIONS
   VectorType v;
   typedef typename VectorType::size_type    size_type;
   typedef typename VectorType::value_type   value_type;
   size_type const max = Unsigned(-1);
   v.resize(5);
   v.resize(max);
   BOOST_TEST_THROWS(v.resize(max+1),                    std::exception);
   BOOST_TEST_THROWS(v.push_back(value_type(1)),         std::exception);
   BOOST_TEST_THROWS(v.insert(v.begin(), value_type(1)), std::exception);
   BOOST_TEST_THROWS(v.emplace(v.begin(), value_type(1)),std::exception);
   BOOST_TEST_THROWS(v.reserve(max+1),                   std::exception);
   BOOST_TEST_THROWS(VectorType v2(max+1),               std::exception);
   #endif
}

template<class Unsigned>
void test_stored_size_type()
{
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
   using options_t = vector_options_t< stored_size<Unsigned> >;
   #else
   typedef typename vector_options
      < stored_size<Unsigned> >::type options_t;
   #endif

   //Test first with a typical allocator
   {
      typedef vector<unsigned char, new_allocator<unsigned char>, options_t> vector_t;
      test_stored_size_type_impl<Unsigned, vector_t>();
   }
   //Test with a V2 allocator
   {
      typedef vector<unsigned char, allocator<unsigned char>, options_t> vector_t;
      test_stored_size_type_impl<Unsigned, vector_t>();
   }
}

void test_growth_factor_50()
{
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
   using options_t = vector_options_t< growth_factor<growth_factor_50> >;
   #else
   typedef vector_options
      < growth_factor<growth_factor_50> >::type options_t;
   #endif

   vector<int, new_allocator<int>, options_t> v;

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
   using options_t = vector_options_t< growth_factor<growth_factor_60> >;
   #else
   typedef vector_options
      < growth_factor<growth_factor_60> >::type options_t;
   #endif

   vector<int, new_allocator<int>, options_t> v;

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
   using options_t = vector_options_t< growth_factor<growth_factor_100> >;
   #else
   typedef vector_options
      < growth_factor<growth_factor_100> >::type options_t;
   #endif

   vector<int, new_allocator<int>, options_t> v;

   v.resize(5);
   v.resize(v.capacity());
   std::size_t old_capacity = v.capacity();
   v.push_back(0);
   std::size_t new_capacity = v.capacity();
   BOOST_TEST(new_capacity == 2*old_capacity);
}

int main()
{
   test_growth_factor_50();
   test_growth_factor_60();
   test_growth_factor_100();
   test_stored_size_type<unsigned char>();
   test_stored_size_type<unsigned short>();
   return ::boost::report_errors();
}
