
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/is_identifier.hpp>
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
    
    #define BOOST_VMD_REGISTER_zzz (zzz)
    #define BOOST_VMD_DETECT_zzz_zzz
    #define BOOST_VMD_REGISTER_somevalue (somevalue)
    #define BOOST_VMD_DETECT_somevalue_somevalue
    #define BOOST_VMD_REGISTER_num (num)
    #define BOOST_VMD_DETECT_num_num
    #define BOOST_VMD_REGISTER_eeb (eeb)
    #define BOOST_VMD_DETECT_eeb_eeb
    #define BOOST_VMD_REGISTER_grist (grist)
    #define BOOST_VMD_DETECT_grist_grist
  
    BOOST_TEST(BOOST_VMD_IS_IDENTIFIER(BOOST_PP_TUPLE_ELEM(2,A_TUPLE),zzz));
    BOOST_TEST(BOOST_VMD_IS_IDENTIFIER(JDATA,somevalue));
    BOOST_TEST(BOOST_VMD_IS_IDENTIFIER(BOOST_PP_SEQ_ELEM(0,A_SEQ),num));
    BOOST_TEST(BOOST_VMD_IS_IDENTIFIER(BOOST_PP_LIST_AT(A_LIST,0),eeb));
    BOOST_TEST(BOOST_VMD_IS_IDENTIFIER(BOOST_PP_LIST_AT(A_LIST,1),grist));
    BOOST_TEST(!BOOST_VMD_IS_IDENTIFIER(JDATA,babble));
    BOOST_TEST(!BOOST_VMD_IS_IDENTIFIER(BOOST_PP_LIST_AT(A_LIST,1),eeb));
    BOOST_TEST_EQ(BOOST_VMD_IS_IDENTIFIER(BOOST_PP_SEQ_ELEM(0,A_SEQ),(babble,num,whatever)),1);
    
#else

BOOST_ERROR("No variadic macro support");
  
#endif

      return boost::report_errors();
  
      }
