/*
 *
 * Copyright (c) 2004
 * John Maddock
 *
 * Use, modification and distribution are subject to the 
 * Boost Software License, Version 1.0. (See accompanying file 
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */

 /*
  *   LOCATION:    see http://www.boost.org for most recent version.
  *   FILE         object_cache_test.cpp
  *   VERSION      see <boost/version.hpp>
  *   DESCRIPTION: Test code for a generic object cache.
  */
#include <boost/regex/config.hpp>
#ifdef BOOST_REGEX_CXX03
#include <boost/regex/v4/object_cache.hpp>
#define SP_NS boost
#else
#include <boost/regex/v5/object_cache.hpp>
#define SP_NS std
#endif
#include <boost/detail/lightweight_main.hpp>
#include "../test_macros.hpp"

class test_object
{
public:
   test_object(int i)
      : m_value(i)
   {
      ++s_count;
   }
   int value()const
   {
      return m_value;
   }
   static int count()
   {
      return s_count;
   }
private:
   int m_value;
   static int s_count;
};

int test_object::s_count = 0;

static const int max_cache_size = 5;

int cpp_main(int /*argc*/, char * /*argv*/[])
{
   int i;
   for(i = 0; i < 20; ++i)
   {
      SP_NS::shared_ptr<const test_object> p = boost::object_cache<int, test_object>::get(i, max_cache_size);
      BOOST_CHECK(p->value() == i);
      p = boost::object_cache<int, test_object>::get(i, max_cache_size);
      BOOST_CHECK(p->value() == i);
      if(i)
      {
         p = boost::object_cache<int, test_object>::get(i-1, max_cache_size);
         BOOST_CHECK(p->value() == i-1);
      }
   }
   int current_count = test_object::count();
   for(int j = 0; j < 10; ++j)
   {
      for(i = 20 - max_cache_size; i < 20; ++i)
      {
         SP_NS::shared_ptr<const test_object> p = boost::object_cache<int, test_object>::get(i, max_cache_size);
         BOOST_CHECK(p->value() == i);
         p = boost::object_cache<int, test_object>::get(i, max_cache_size);
         BOOST_CHECK(p->value() == i);
      }
   }
   BOOST_CHECK(current_count == test_object::count());
   return 0;
}


