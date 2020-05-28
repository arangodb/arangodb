//  Copyright 2015 Vicente J. Botet Escriba
//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt
//  See http://www.boost.org/libs/chrono for documentation.

//#define BOOST_CHRONO_VERSION 1
#define BOOST_CHRONO_VERSION 2
#include <iostream>
#include <boost/rational.hpp>
#include <boost/chrono/chrono.hpp>
//#define BOOST_CHRONO_DONT_PROVIDES_DEPRECATED_IO_SINCE_V2_0_0
#include <boost/chrono/chrono_io.hpp>

int main()
{
  {
    typedef boost::chrono::duration<float> RationalSeconds;
    RationalSeconds d(0.5);
    std::cout << d << std::endl;
  }
  {
    typedef boost::chrono::duration<boost::rational<int> > RationalSeconds;
    RationalSeconds d;
    std::cout << d << std::endl;
  }
  return 0;
}
