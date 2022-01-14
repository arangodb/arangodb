
//  (C) Copyright Edward Diener 2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/tuple/pop_back.hpp>
#include <boost/vmd/is_empty.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/tuple/size.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define A_TUPLE (1,2,3,4)
  #define AN_SE_TUPLE (1)
  
  BOOST_TEST_EQ(BOOST_PP_TUPLE_ELEM(2,BOOST_VMD_TUPLE_POP_BACK(A_TUPLE)),3);
  BOOST_TEST_EQ(BOOST_PP_TUPLE_SIZE(BOOST_VMD_TUPLE_POP_BACK(A_TUPLE)),3);
  BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_VMD_TUPLE_POP_BACK(AN_SE_TUPLE)));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
