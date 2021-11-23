
//  (C) Copyright Edward Diener 2020
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/is_general_identifier.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/preprocessor/list/at.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/tuple/elem.hpp>

int main()
      {
  
#if BOOST_PP_VARIADICS

    #define A_TUPLE (*,#,zzz)
    #define JDATA somevalue
    #define A_SEQ (num)(%)(#)
    #define A_LIST (eeb,(grist,(&,BOOST_PP_NIL)))

    BOOST_TEST(BOOST_VMD_IS_GENERAL_IDENTIFIER(BOOST_PP_TUPLE_ELEM(2,A_TUPLE)));
    BOOST_TEST(BOOST_VMD_IS_GENERAL_IDENTIFIER(JDATA));
    BOOST_TEST(BOOST_VMD_IS_GENERAL_IDENTIFIER(BOOST_PP_SEQ_ELEM(0,A_SEQ)));
    BOOST_TEST(BOOST_VMD_IS_GENERAL_IDENTIFIER(BOOST_PP_LIST_AT(A_LIST,0)));
    BOOST_TEST(BOOST_VMD_IS_GENERAL_IDENTIFIER(BOOST_PP_LIST_AT(A_LIST,1)));
    BOOST_TEST(!BOOST_VMD_IS_GENERAL_IDENTIFIER());
    BOOST_TEST(!BOOST_VMD_IS_GENERAL_IDENTIFIER(A_TUPLE));
    BOOST_TEST(!BOOST_VMD_IS_GENERAL_IDENTIFIER(A_SEQ));
    BOOST_TEST(!BOOST_VMD_IS_GENERAL_IDENTIFIER(A_LIST));
    
#else

BOOST_ERROR("No variadic macro support");
  
#endif

      return boost::report_errors();
  
      }
