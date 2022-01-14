//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#define BOOST_ENABLE_ASSERT_HANDLER
#include <boost/container/static_vector.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/no_exceptions_support.hpp>
#include <new> //for bad_alloc
#include <boost/assert.hpp>
#include <cstdlib>
using namespace boost::container;

//User-defined assertion to test throw_on_overflow
struct throw_on_overflow_off
{};

namespace boost {
   void assertion_failed(char const *, char const *, char const *, long)
   {
      #ifdef BOOST_NO_EXCEPTIONS
      std::abort();
      #else
      throw throw_on_overflow_off();
      #endif
   }

   void assertion_failed_msg(char const *, char const *, char const *, char const *, long )
   {
      #ifdef BOOST_NO_EXCEPTIONS
      std::abort();
      #else
      throw throw_on_overflow_off();
      #endif
   }
}

void test_alignment()
{
   const std::size_t Capacity = 10u;
   {  //extended alignment
      const std::size_t extended_alignment = sizeof(int)*4u;
      BOOST_STATIC_ASSERT(extended_alignment > dtl::alignment_of<int>::value);
      #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
      using options_t = static_vector_options_t< inplace_alignment<extended_alignment> >;
      #else
      typedef static_vector_options
         < inplace_alignment<extended_alignment> >::type options_t;
      #endif

      static_vector<int, Capacity, options_t> v;
      v.resize(v.capacity());
      BOOST_ASSERT((reinterpret_cast<std::size_t>(&v[0]) % extended_alignment) == 0);
   }
   {  //default alignment
      #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
      using options_t = static_vector_options_t< inplace_alignment<0> >;
      #else
      typedef static_vector_options< inplace_alignment<0> >::type options_t;
      #endif

      static_vector<int, Capacity, options_t> v;
      v.resize(v.capacity());
      BOOST_ASSERT((reinterpret_cast<std::size_t>(&v[0]) % dtl::alignment_of<int>::value) == 0);
   }
}

void test_throw_on_overflow()
{
   #if !defined(BOOST_NO_EXCEPTIONS)
   const std::size_t Capacity = 10u;
   {  //throw_on_overflow == true, expect bad_alloc
      #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
      using options_t = static_vector_options_t< throw_on_overflow<true> >;
      #else
      typedef static_vector_options
         < throw_on_overflow<true> >::type options_t;
      #endif

      static_vector<int, Capacity, options_t> v;

      v.resize(Capacity);
      bool expected_type_thrown = false;

      BOOST_TRY{
         v.push_back(0);
      }
      BOOST_CATCH(bad_alloc_t&)
      {
         expected_type_thrown = true;
      }
      BOOST_CATCH(...)
      {}
      BOOST_CATCH_END

      BOOST_TEST(expected_type_thrown == true);
      BOOST_TEST(v.capacity() == Capacity);
   }
   {  //throw_on_overflow == false, test it through BOOST_ASSERT
      //even in release mode (BOOST_ENABLE_ASSERT_HANDLER), and throwing
      //a special type in that assertion.
      #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
      using options_t = static_vector_options_t< throw_on_overflow<false> >;
      #else
      typedef static_vector_options< throw_on_overflow<false> >::type options_t;
      #endif

      static_vector<int, Capacity, options_t> v;

      v.resize(Capacity);
      bool expected_type_thrown = false;

      BOOST_TRY{
         v.push_back(0);
      }
      BOOST_CATCH(throw_on_overflow_off)
      {
         expected_type_thrown = true;
      }
      BOOST_CATCH(...)
      {}
      BOOST_CATCH_END

      BOOST_TEST(expected_type_thrown == true);
      BOOST_TEST(v.capacity() == Capacity);
   }
   #endif
}

int main()
{
   test_alignment();
   test_throw_on_overflow();
   return ::boost::report_errors();
}
