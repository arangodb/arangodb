
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/assert.hpp>
#include <boost/vmd/elem.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/preprocessor/comparison/equal.hpp>
#include <boost/preprocessor/tuple/elem.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define A_TUPLE (*,#,zzz ())
  
  #define BOOST_VMD_REGISTER_zzz (zzz)
  #define BOOST_VMD_DETECT_zzz_zzz
  
  BOOST_VMD_ASSERT
      (
      BOOST_PP_EQUAL
          (
        BOOST_PP_TUPLE_ELEM
            (
            2,
            BOOST_VMD_ELEM(0,BOOST_PP_TUPLE_ELEM(2,A_TUPLE),(dummy1,zzz),BOOST_VMD_RETURN_AFTER,BOOST_VMD_RETURN_INDEX,BOOST_VMD_TYPE_IDENTIFIER)
            ),
        0
          ),
      BOOST_VMD_TEST_FAIL_IDENTIFIER
      )
      
#endif

  return boost::report_errors();
  
  }
