//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//  Adaptation to Boost of the libcxx
//  Copyright 2010 Vicente J. Botet Escriba
//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

// test ratio_power

#define BOOST_RATIO_EXTENSIONS
#include <boost/ratio/ratio.hpp>

#if !defined(BOOST_NO_CXX11_STATIC_ASSERT)
#define NOTHING ""
#endif

void test()
{
  {
    typedef boost::ratio<1, 2> R1;
    typedef boost::ratio_power<R1, 1> R;
    BOOST_RATIO_STATIC_ASSERT(R::num == 1 && R::den == 2, NOTHING, ());
  }
  {
    typedef boost::ratio<1, 2> R1;
    typedef boost::ratio_power<R1, -1> R;
    BOOST_RATIO_STATIC_ASSERT(R::num == 2 && R::den == 1, NOTHING, ());
  }
  {
    typedef boost::ratio<1, 2> R1;
    typedef boost::ratio_power<R1, 0> R;
    BOOST_RATIO_STATIC_ASSERT(R::num == 1 && R::den == 1, NOTHING, ());
  }
  {
    typedef boost::ratio<-1, 2> R1;
    typedef boost::ratio_power<R1, 2> R;
    BOOST_RATIO_STATIC_ASSERT(R::num == 1 && R::den == 4, NOTHING, ());
  }
  {
    typedef boost::ratio<1, -2> R1;
    typedef boost::ratio_power<R1, 2> R;
    BOOST_RATIO_STATIC_ASSERT(R::num == 1 && R::den == 4, NOTHING, ());
  }
  {
    typedef boost::ratio<2, 3> R1;
    typedef boost::ratio_power<R1, 2> R;
    BOOST_RATIO_STATIC_ASSERT(R::num == 4 && R::den == 9, NOTHING, ());
  }
  {
    typedef boost::ratio<2, 3> R1;
    typedef boost::ratio_power<R1, -2> R;
    BOOST_RATIO_STATIC_ASSERT(R::num == 9 && R::den == 4, NOTHING, ());
  }
}
