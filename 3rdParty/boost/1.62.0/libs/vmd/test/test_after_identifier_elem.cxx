
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/elem.hpp>
#include <boost/vmd/is_empty.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/preprocessor/list/at.hpp>
#include <boost/preprocessor/punctuation/is_begin_parens.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/tuple/elem.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define BOOST_VMD_REGISTER_ggh (ggh)
  #define BOOST_VMD_REGISTER_dvd (dvd)
  #define BOOST_VMD_REGISTER_ccc (ccc)
  #define BOOST_VMD_REGISTER_aname (aname)
  
  #define BOOST_VMD_DETECT_ggh_ggh
  #define BOOST_VMD_DETECT_ccc_ccc
  #define BOOST_VMD_DETECT_aname_aname
  
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
  
  #define ANIDENTIFIER ggh
  #define ANIDENTIFIER2 dvd
  #define ANIDENTIFIER3 ccc
  #define ANIDENTIFIER5 aname
  #define ANUMBER 249
  #define ANUMBER2 17
  #define ASEQ (25)(26)(27)
  #define ATUPLE (0,1,2,3,((a,b))((c,d))((e))((f,g,h)))
  #define ALIST (0,(1,(2,(3,BOOST_PP_NIL))))
  #define ANARRAY (3,(a,b,38))
  #define ANARRAY2 (5,(c,d,133,22,15))
  #define ASEQUENCE ANUMBER ALIST ATUPLE ANIDENTIFIER ANARRAY ASEQ
  #define ASEQUENCE2 ANIDENTIFIER2 ANARRAY2 ASEQ ALIST ANUMBER ATUPLE
  #define ASEQUENCE3 ASEQ ANUMBER2 ANIDENTIFIER3 ATUPLE
  #define ASEQUENCE4
  #define ASEQUENCE5 ASEQ ANUMBER ATUPLE ANIDENTIFIER5

  #define A_TUPLE (*,#,zzz ())
  #define JDATA somevalue
  #define A_SEQ (num (split) clear)(%)(#)
  #define A_LIST (eeb (5),(grist,(&,BOOST_PP_NIL)))

  BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_VMD_ELEM(0,ATUPLE,BOOST_VMD_RETURN_ONLY_AFTER,BOOST_VMD_TYPE_IDENTIFIER)));
  BOOST_TEST(!BOOST_VMD_IS_EMPTY(BOOST_VMD_ELEM(3,ASEQUENCE,BOOST_VMD_RETURN_ONLY_AFTER,BOOST_VMD_TYPE_IDENTIFIER)));
  BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_VMD_ELEM(0,ASEQUENCE2,BOOST_VMD_RETURN_ONLY_AFTER,dvd,BOOST_VMD_TYPE_IDENTIFIER)));
  BOOST_TEST_EQ(BOOST_PP_TUPLE_ELEM(1,BOOST_VMD_ELEM(2,ASEQUENCE3,BOOST_VMD_RETURN_ONLY_AFTER,BOOST_VMD_TYPE_IDENTIFIER)),1);
  BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_VMD_ELEM(0,ASEQUENCE4,BOOST_VMD_RETURN_ONLY_AFTER,BOOST_VMD_TYPE_IDENTIFIER)));
  BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_VMD_ELEM(3,ASEQUENCE5,BOOST_VMD_RETURN_ONLY_AFTER,BOOST_VMD_TYPE_IDENTIFIER)));
  
  BOOST_TEST
      (
      BOOST_PP_IS_BEGIN_PARENS
          (
        BOOST_VMD_ELEM(0,BOOST_PP_TUPLE_ELEM(2,A_TUPLE),zzz,BOOST_VMD_RETURN_ONLY_AFTER,BOOST_VMD_TYPE_IDENTIFIER)
          )
      );
  
  BOOST_TEST
      (
      BOOST_VMD_IS_EMPTY
          (
        BOOST_VMD_ELEM(0,JDATA,somevalue,BOOST_VMD_RETURN_ONLY_AFTER,BOOST_VMD_TYPE_IDENTIFIER)
          )
      );
  
  BOOST_TEST
      (
      BOOST_PP_IS_BEGIN_PARENS
          (
          BOOST_VMD_ELEM(0,BOOST_PP_SEQ_ELEM(0,A_SEQ),num,BOOST_VMD_RETURN_ONLY_AFTER,BOOST_VMD_TYPE_IDENTIFIER)
          )
      );
  
  BOOST_TEST_EQ
      (
      BOOST_PP_TUPLE_ELEM
          (
          0,
         BOOST_VMD_ELEM(0,BOOST_PP_LIST_AT(A_LIST,0),(eeb),BOOST_VMD_RETURN_ONLY_AFTER,BOOST_VMD_TYPE_IDENTIFIER)
          ),
      5
      );
  
  BOOST_TEST
      (
      BOOST_VMD_IS_EMPTY
          (
        BOOST_VMD_ELEM(0,BOOST_PP_LIST_AT(A_LIST,1),grist,BOOST_VMD_RETURN_ONLY_AFTER,BOOST_VMD_TYPE_IDENTIFIER)
          )
      );
      
  BOOST_TEST
      (
      BOOST_VMD_IS_EMPTY
          (
          BOOST_VMD_ELEM(0,JDATA,babble,BOOST_VMD_RETURN_ONLY_AFTER,BOOST_VMD_TYPE_IDENTIFIER)
          )
      );
  
  BOOST_TEST
      (
      BOOST_VMD_IS_EMPTY
          (
          BOOST_VMD_ELEM(0,BOOST_PP_LIST_AT(A_LIST,1),eeb,BOOST_VMD_RETURN_ONLY_AFTER,BOOST_VMD_TYPE_IDENTIFIER)
          )
      );
      
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
