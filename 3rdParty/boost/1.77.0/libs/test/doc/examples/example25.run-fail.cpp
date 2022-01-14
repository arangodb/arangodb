//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#include <stdexcept>
#include <boost/test/included/prg_exec_monitor.hpp> 

int foo() { throw std::runtime_error( "big trouble" ); }

int cpp_main( int, char* [] ) // note the name
{
  foo();

  return 0;
}
//]
