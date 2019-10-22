
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

  #define ATYPE BOOST_VMD_TYPE_TUPLE
  #define ATYPE2 BOOST_VMD_TYPE_NUMBER
  #define ATYPE3 BOOST_VMD_TYPE_TUPLE
  
  BOOST_TEST(BOOST_VMD_EQUAL(ATYPE,ATYPE3));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ATYPE,ATYPE2));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
