//  example/stopwatch_example.cpp  ---------------------------------------------------//
//  Copyright Beman Dawes 2006, 2008
//  Copyright 2009-2011 Vicente J. Botet Escriba
//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt
//  See http://www.boost.org/libs/chrono/stopwatches for documentation.

//#include <iostream>
#include <boost/chrono/stopwatches/strict_stopwatch.hpp>
#include <boost/chrono/stopwatches/reporters/stopwatch_reporter.hpp>
#include <boost/chrono/stopwatches/reporters/system_default_formatter.hpp>
#include <boost/chrono/chrono_io.hpp>
#include <cmath>

using namespace boost::chrono;

int f1(long j)
{
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  stopwatch_reporter<strict_stopwatch<> > sw;
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;

  for ( long i = 0; i < j; ++i )
    std::sqrt( 123.456L );  // burn some time

  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  return 0;
}
int main()
{
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  stopwatch_reporter<strict_stopwatch<> > sw;
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;

  f1(1000);
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  f1(2000);
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  f1(3000);
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  return 0;
}
