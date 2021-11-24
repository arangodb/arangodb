/* Copyright (C) 2011 John Maddock
* 
* Use, modification and distribution is subject to the 
* Boost Software License, Version 1.0. (See accompanying
* file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
*/

#include <iostream>
#include <boost/pool/pool_alloc.hpp>
#include <boost/thread.hpp>
#if defined(BOOST_MSVC) && (BOOST_MSVC <= 1600)
#pragma warning(push)
#pragma warning(disable: 4244)
// ..\..\boost/random/uniform_int_distribution.hpp(171) :
//   warning C4127: conditional expression is constant
#pragma warning(disable: 4127)
// ..\..\boost/random/detail/polynomial.hpp(315) :
//   warning C4267: 'argument' : conversion from 'size_t'
//   to 'int', possible loss of data
#pragma warning(disable: 4267)
#endif
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#if defined(BOOST_MSVC) && (BOOST_MSVC <= 1600)
#pragma warning(pop)
#endif

void run_tests()
{
   boost::random::mt19937 gen;
   boost::random::uniform_int_distribution<> dist(-10, 10);
   std::list<int, boost::fast_pool_allocator<int> > l;

   for(int i = 0; i < 100; ++i)
      l.push_back(i);

   for(int i = 0; i < 20000; ++i)
   {
      int val = dist(gen);
      if(val < 0)
      {
         while(val && l.size())
         {
            l.pop_back();
            ++val;
         }
      }
      else
      {
         while(val)
         {
            l.push_back(val);
            --val;
         }
      }
   }
}

int main()
{
   std::list<boost::shared_ptr<boost::thread> > threads;
   for(int i = 0; i < 10; ++i)
   {
      try{
         threads.push_back(boost::shared_ptr<boost::thread>(new boost::thread(&run_tests)));
      }
      catch(const std::exception& e)
      {
         std::cerr << "<note>Thread creation failed with message: " << e.what() << "</note>" << std::endl;
      }
   }
   std::list<boost::shared_ptr<boost::thread> >::const_iterator a(threads.begin()), b(threads.end());
   while(a != b)
   {
      (*a)->join();
      ++a;
   }
   return 0;
}

