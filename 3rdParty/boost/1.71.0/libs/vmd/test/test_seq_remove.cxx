
//  (C) Copyright Edward Diener 2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/seq/remove.hpp>
#include <boost/vmd/is_empty.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/seq/size.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define A_SEQ (1)(2)(3)(4)
  #define AN_OE_SEQ (1)
  
  BOOST_TEST_EQ(BOOST_PP_SEQ_ELEM(2,BOOST_VMD_SEQ_REMOVE(A_SEQ,2)),4);
  BOOST_TEST_EQ(BOOST_PP_SEQ_SIZE(BOOST_VMD_SEQ_REMOVE(A_SEQ,1)),3);
  BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_VMD_SEQ_REMOVE(AN_OE_SEQ,0)));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
