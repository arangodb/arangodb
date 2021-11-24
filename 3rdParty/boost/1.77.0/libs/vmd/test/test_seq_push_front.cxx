
//  (C) Copyright Edward Diener 2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/seq/push_front.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/seq/size.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define A_SEQ (1)(2)(3)(4)
  #define AN_EMPTY_SEQ
  
  BOOST_TEST_EQ(BOOST_PP_SEQ_ELEM(0,BOOST_VMD_SEQ_PUSH_FRONT(A_SEQ,10)),10);
  BOOST_TEST_EQ(BOOST_PP_SEQ_SIZE(BOOST_VMD_SEQ_PUSH_FRONT(A_SEQ,10)),5);
  BOOST_TEST_EQ(BOOST_PP_SEQ_ELEM(0,BOOST_VMD_SEQ_PUSH_FRONT(AN_EMPTY_SEQ,20)),20);
  BOOST_TEST_EQ(BOOST_PP_SEQ_SIZE(BOOST_VMD_SEQ_PUSH_FRONT(AN_EMPTY_SEQ,20)),1);
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
