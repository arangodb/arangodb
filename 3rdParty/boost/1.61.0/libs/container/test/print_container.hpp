//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_PRINTCONTAINER_HPP
#define BOOST_PRINTCONTAINER_HPP

#include <boost/container/detail/config_begin.hpp>
#include <iostream>

namespace boost{
namespace container {
namespace test{

//Function to dump data
template<class MyBoostCont
        ,class MyStdCont>
void PrintContainers(MyBoostCont *boostcont, MyStdCont *stdcont)
{
   typename MyBoostCont::iterator itboost = boostcont->begin(), itboostend = boostcont->end();
   typename MyStdCont::iterator itstd = stdcont->begin(), itstdend = stdcont->end();

   std::cout << "MyBoostCont" << std::endl;
   for(; itboost != itboostend; ++itboost){
      std::cout << *itboost << std::endl;
   }
   std::cout << "MyStdCont" << std::endl;

   for(; itstd != itstdend; ++itstd){
      std::cout << *itstd << std::endl;
   }
}

}  //namespace test{
}  //namespace container {
}  //namespace boost{

#include <boost/container/detail/config_end.hpp>

#endif //#ifndef BOOST_PRINTCONTAINER_HPP
