
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/equal.hpp>
#include <boost/vmd/not_equal.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define ANUMBER 249
  #define ANUMBER2 17
  #define ANUMBER3 249
  
  BOOST_TEST(BOOST_VMD_EQUAL(ANUMBER,ANUMBER3));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ANUMBER,ANUMBER2));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
