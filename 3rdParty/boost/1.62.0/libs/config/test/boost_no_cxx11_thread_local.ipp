//  (C) Copyright John Maddock 2012. 
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/config for most recent version.

//  MACRO:         BOOST_NO_CXX11_THREAD_LOCAL
//  TITLE:         thread_local
//  DESCRIPTION:   The compiler supports the thread_local storage specifier.

#include <string>


namespace boost_no_cxx11_thread_local{

int test()
{
   static thread_local std::string local("hello");
   return 0;
}

}

