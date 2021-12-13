
//  (C) Copyright Edward Diener 2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/seq/to_array.hpp>
#include <boost/vmd/is_empty_array.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/preprocessor/array/elem.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define A_SEQ (1)(2)(3)(4)
  #define AN_EMPTY_SEQ
  
  BOOST_TEST_EQ(BOOST_PP_ARRAY_ELEM(2,BOOST_VMD_SEQ_TO_ARRAY(A_SEQ)),3);
  BOOST_TEST(BOOST_VMD_IS_EMPTY_ARRAY(BOOST_VMD_SEQ_TO_ARRAY(AN_EMPTY_SEQ)));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
