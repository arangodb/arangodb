
//  (C) Copyright Edward Diener 2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/tuple/push_back.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/tuple/size.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define A_TUPLE (1,2,3,4)
  #define AN_EMPTY_TUPLE
  
  BOOST_TEST_EQ(BOOST_PP_TUPLE_ELEM(4,BOOST_VMD_TUPLE_PUSH_BACK(A_TUPLE,10)),10);
  BOOST_TEST_EQ(BOOST_PP_TUPLE_SIZE(BOOST_VMD_TUPLE_PUSH_BACK(A_TUPLE,10)),5);
  BOOST_TEST_EQ(BOOST_PP_TUPLE_ELEM(0,BOOST_VMD_TUPLE_PUSH_BACK(AN_EMPTY_TUPLE,20)),20);
  BOOST_TEST_EQ(BOOST_PP_TUPLE_SIZE(BOOST_VMD_TUPLE_PUSH_BACK(AN_EMPTY_TUPLE,20)),1);
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
