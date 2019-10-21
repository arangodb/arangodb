//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTERPROCESS_TEST_PRINTCONTAINER_HPP
#define BOOST_INTERPROCESS_TEST_PRINTCONTAINER_HPP

#include <boost/interprocess/detail/config_begin.hpp>
#include <iostream>

namespace boost{
namespace interprocess{
namespace test{

template<class Container>
void PrintContents(const Container &cont, const char *contName)
{
   std::cout<< "Printing contents of " << contName << std::endl;
   typename Container::iterator b(cont.begin()), e(cont.end());
   for(; b != e; ++b){
      std::cout << *b << " ";
   }
   std::cout<< std::endl << std::endl;
}

//Function to dump data
template<class MyShmCont, class MyStdCont>
void PrintContainers(MyShmCont *shmcont, MyStdCont *stdcont)
{
   typename MyShmCont::iterator itshm = shmcont->begin(), itshmend = shmcont->end();
   typename MyStdCont::iterator itstd = stdcont->begin(), itstdend = stdcont->end();

   std::cout << "MyShmCont" << std::endl;
   for(; itshm != itshmend; ++itshm){
      std::cout << *itshm << std::endl;
   }
   std::cout << "MyStdCont" << std::endl;

   for(; itstd != itstdend; ++itstd){
      std::cout << *itstd << std::endl;
   }
}

}  //namespace test{
}  //namespace interprocess{
}  //namespace boost{

#include <boost/interprocess/detail/config_end.hpp>

#endif //#ifndef BOOST_INTERPROCESS_TEST_PRINTCONTAINER_HPP
