//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#include <iostream>
#include <boost/test/included/prg_exec_monitor.hpp> 

int cpp_main( int, char* [] ) // note name cpp_main, not main.
{
  std::cout << "Hello, world\n";

  return 0;
}
//]
