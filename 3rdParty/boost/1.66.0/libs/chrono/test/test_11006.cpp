//  Copyright 2015 Vicente J. Botet Escriba
//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt
//  See http://www.boost.org/libs/chrono for documentation.

//#define BOOST_CHRONO_PROVIDES_DEPRECATED_IO_SINCE_V2_0_0
#define BOOST_CHRONO_VERSION 2
#include <iostream>
#include <boost/chrono/io/time_point_io.hpp>


int main() {
  {
    boost::chrono::time_fmt_io_saver<> tmp(std::cout);
  }
  {
    boost::chrono::time_fmt_io_saver<> tmp(std::cout, "%Y-%m-%d %H:%M:%S");
  }
  return 0;
}

