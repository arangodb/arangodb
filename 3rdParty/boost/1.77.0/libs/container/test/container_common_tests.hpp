//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2014-2014. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_TEST_CONTAINER_COMMON_TESTS_HPP
#define BOOST_CONTAINER_TEST_CONTAINER_COMMON_TESTS_HPP

#include <boost/container/detail/config_begin.hpp>

namespace boost{
namespace container {
namespace test{


template<class Container>
const Container &as_const(Container &c)
{  return c;   }

//nth, index_of
template<class Container>
bool test_nth_index_of(Container &c)
{
   typename Container::iterator it;
   typename Container::const_iterator cit;
   typename Container::size_type sz, csz;
   //index 0
   it = c.nth(0);
   sz = c.index_of(it);
   cit = (as_const)(c).nth(0);
   csz = (as_const)(c).index_of(cit);

   if(it != c.begin())
      return false;
   if(cit != c.cbegin())
      return false;
   if(sz != 0)
      return false;
   if(csz != 0)
      return false;

   //index size()/2
   const typename Container::size_type sz_div_2 = c.size()/2;
   it = c.nth(sz_div_2);
   sz = c.index_of(it);
   cit = (as_const)(c).nth(sz_div_2);
   csz = (as_const)(c).index_of(cit);

   if(it != (c.begin()+sz_div_2))
      return false;
   if(cit != (c.cbegin()+sz_div_2))
      return false;
   if(sz != sz_div_2)
      return false;
   if(csz != sz_div_2)
      return false;

   //index size()
   it = c.nth(c.size());
   sz = c.index_of(it);
   cit = (as_const)(c).nth(c.size());
   csz = (as_const)(c).index_of(cit);

   if(it != c.end())
      return false;
   if(cit != c.cend())
      return false;
   if(sz != c.size())
      return false;
   if(csz != c.size())
      return false;
   return true;
}

}  //namespace test{
}  //namespace container {
}  //namespace boost{

#include <boost/container/detail/config_end.hpp>

#endif //#ifndef BOOST_CONTAINER_TEST_CONTAINER_COMMON_TESTS_HPP
