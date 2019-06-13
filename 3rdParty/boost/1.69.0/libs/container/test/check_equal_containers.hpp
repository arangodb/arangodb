//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2006. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_TEST_CHECK_EQUAL_CONTAINER_HPP
#define BOOST_CONTAINER_TEST_CHECK_EQUAL_CONTAINER_HPP

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/pair.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/move/unique_ptr.hpp>

#include <cstddef>
#include <boost/container/detail/iterator.hpp>

namespace boost{
namespace container {
namespace test{

template< class T1, class T2>
bool CheckEqual( const T1 &t1, const T2 &t2
               , typename boost::container::dtl::enable_if_c
                  <!boost::container::dtl::is_pair<T1>::value &&
                   !boost::container::dtl::is_pair<T2>::value
                  >::type* = 0)
{  return t1 == t2;  }


template<class T1, class T2, class C1, class C2>
bool CheckEqualIt( const T1 &i1, const T2 &i2, const C1 &c1, const C2 &c2 )
{
   bool c1end = i1 == c1.end();
   bool c2end = i2 == c2.end();
   if( c1end != c2end ){
      return false;
   }
   else if(c1end){
      return true;
   }
   else{
      return CheckEqual(*i1, *i2);
   }
}

template< class Pair1, class Pair2>
bool CheckEqual( const Pair1 &pair1, const Pair2 &pair2
               , typename boost::container::dtl::enable_if_c
                  <boost::container::dtl::is_pair<Pair1>::value &&
                   boost::container::dtl::is_pair<Pair2>::value
                  >::type* = 0)
{
   return CheckEqual(pair1.first, pair2.first) && CheckEqual(pair1.second, pair2.second);
}

//Function to check if both containers are equal
template<class ContA
        ,class ContB>
bool CheckEqualContainers(const ContA &cont_a, const ContB &cont_b)
{
   if(cont_a.size() != cont_b.size())
      return false;

   typename ContA::const_iterator itcont_a(cont_a.begin()), itcont_a_end(cont_a.end());
   typename ContB::const_iterator itcont_b(cont_b.begin()), itcont_b_end(cont_b.end());;
   typename ContB::size_type dist = (typename ContB::size_type)boost::container::iterator_distance(itcont_a, itcont_a_end);
   if(dist != cont_a.size()){
      return false;
   }
   typename ContA::size_type dist2 = (typename ContA::size_type)boost::container::iterator_distance(itcont_b, itcont_b_end);
   if(dist2 != cont_b.size()){
      return false;
   }
   std::size_t i = 0;
   for(; itcont_a != itcont_a_end; ++itcont_a, ++itcont_b, ++i){
      if(!CheckEqual(*itcont_a, *itcont_b))
         return false;
   }
   return true;
}

template<class MyBoostCont
        ,class MyStdCont>
bool CheckEqualPairContainers(const MyBoostCont &boostcont, const MyStdCont &stdcont)
{
   if(boostcont.size() != stdcont.size())
      return false;

   typedef typename MyBoostCont::key_type      key_type;
   typedef typename MyBoostCont::mapped_type   mapped_type;

   typename MyBoostCont::const_iterator itboost(boostcont.begin()), itboostend(boostcont.end());
   typename MyStdCont::const_iterator itstd(stdcont.begin());
   for(; itboost != itboostend; ++itboost, ++itstd){
      key_type k(itstd->first);
      if(itboost->first != k)
         return false;

      mapped_type m(itstd->second);
      if(itboost->second != m)
         return false;
   }
   return true;
}

struct less_transparent
{
   typedef void is_transparent;

   template<class T, class U>
   bool operator()(const T &t, const U &u) const
   {
      return t < u;
   }
};

struct equal_transparent
{
   typedef void is_transparent;

   template<class T, class U>
   bool operator()(const T &t, const U &u) const
   {
      return t == u;
   }
};

}  //namespace test{
}  //namespace container {
}  //namespace boost{

#include <boost/container/detail/config_end.hpp>

#endif //#ifndef BOOST_CONTAINER_TEST_CHECK_EQUAL_CONTAINER_HPP
