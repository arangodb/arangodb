// (C) Copyright 2012 Howard Hinnant
// (C) Copyright 2012 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// adapted from the example given by Howard Hinnant in

#define BOOST_THREAD_VERSION 4

#include <iostream>
#include <boost/thread/scoped_thread.hpp>
#include <boost/thread/ostream_buffer.hpp>

void use_cerr()
{
  using namespace boost;
  chrono::steady_clock::time_point tf = chrono::steady_clock::now() + chrono::seconds(5);
  int i = 0;
  while (chrono::steady_clock::now() < tf)
  {
    ostream_buffer<std::ostream> mcerr(std::cerr);
    mcerr.stream() << "logging data to cerr " << i++ << "\n";
    this_thread::sleep_for(chrono::milliseconds(250));
  }
}

void use_cout()
{
  using namespace boost;
  chrono::steady_clock::time_point tf = chrono::steady_clock::now() + chrono::seconds(5);
  int i = 0;
  while (chrono::steady_clock::now() < tf)
  {
    ostream_buffer<std::ostream> mcout(std::cout);
    mcout.stream() << "logging data to cout " << i++ << "\n";
    this_thread::sleep_for(chrono::milliseconds(500));
  }
}

int main()
{
  using namespace boost;

  scoped_thread<> t1(&use_cerr);
  scoped_thread<> t2(&use_cout);
  this_thread::sleep_for(chrono::seconds(2));
  std::string nm = "he, he\n";
  {
    ostream_buffer<std::ostream> mcout(std::cout);
    mcout.stream() << "Enter name: \n";
  }
  t1.join();
  t2.join();
  {
    ostream_buffer<std::ostream> mcout(std::cout);
    mcout.stream() << nm;
  }
  return 0;
}

