
//  (C) Copyright Edward Diener 2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/tuple/to_seq.hpp>
#include <boost/vmd/is_empty.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/preprocessor/seq/elem.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define A_TUPLE (1,2,3,4)
  #define AN_EMPTY_TUPLE
  
  BOOST_TEST_EQ(BOOST_PP_SEQ_ELEM(1,BOOST_VMD_TUPLE_TO_SEQ(A_TUPLE)),2);
  BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_VMD_TUPLE_TO_SEQ(AN_EMPTY_TUPLE)));
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
