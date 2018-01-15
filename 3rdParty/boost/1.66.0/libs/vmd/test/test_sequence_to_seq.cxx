
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/is_empty.hpp>
#include <boost/vmd/to_seq.hpp>
#include <boost/vmd/is_tuple.hpp>
#include <boost/vmd/equal.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/preprocessor/list/at.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/tuple/elem.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define BOOST_VMD_REGISTER_ggh (ggh)
  #define BOOST_VMD_DETECT_ggh_ggh
  #define BOOST_VMD_REGISTER_dvd (dvd)
  #define BOOST_VMD_DETECT_dvd_dvd
  
  #define ANIDENTIFIER ggh
  #define ANIDENTIFIER2 dvd
  #define ANUMBER 249
  #define ANUMBER2 17
  #define ASEQ (25)(26)(27)
  #define ATUPLE (0,1,2,3,((a,b))((c,d))((e))((f,g,h)))
  #define ALIST (0,(1,(2,(3,BOOST_PP_NIL))))
  #define ANARRAY (3,(a,b,38))
  #define ASEQUENCE ANUMBER ALIST ATUPLE ANIDENTIFIER ANARRAY ASEQ
  #define ASEQUENCE2 ANIDENTIFIER2 ASEQ ALIST ANUMBER ATUPLE
  #define ASEQUENCE3 ASEQ ANUMBER2 ATUPLE
  #define ASEQUENCE4
  
  /* TO_SEQ */
  
  BOOST_TEST(BOOST_VMD_EQUAL(BOOST_PP_TUPLE_ELEM(0,BOOST_PP_SEQ_ELEM(1,BOOST_VMD_TO_SEQ(ASEQUENCE3,BOOST_VMD_RETURN_TYPE))),BOOST_VMD_TYPE_NUMBER,BOOST_VMD_TYPE_TYPE));
  BOOST_TEST(BOOST_VMD_IS_TUPLE(BOOST_PP_TUPLE_ELEM(1,BOOST_PP_SEQ_ELEM(4,BOOST_VMD_TO_SEQ(ASEQUENCE2,BOOST_VMD_RETURN_TYPE)))));
  BOOST_TEST_EQ(BOOST_PP_LIST_AT(BOOST_PP_TUPLE_ELEM(1,BOOST_PP_SEQ_ELEM(1,BOOST_VMD_TO_SEQ(ASEQUENCE,BOOST_VMD_RETURN_TYPE))),3),3);
  BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_VMD_TO_SEQ(ASEQUENCE4,BOOST_VMD_RETURN_TYPE)));
  
  /* TO_SEQ_DATA */
  
  BOOST_TEST_EQ(BOOST_PP_SEQ_ELEM(1,BOOST_PP_SEQ_ELEM(1,BOOST_VMD_TO_SEQ(ASEQUENCE2))),26);
  BOOST_TEST(BOOST_VMD_IS_EMPTY(BOOST_VMD_TO_SEQ(ASEQUENCE4)));
  BOOST_TEST_EQ(BOOST_PP_TUPLE_ELEM(1,BOOST_PP_SEQ_ELEM(2,BOOST_VMD_TO_SEQ(ASEQUENCE))),1);
  BOOST_TEST_EQ(BOOST_PP_SEQ_ELEM(1,BOOST_VMD_TO_SEQ(ASEQUENCE3)),17);
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
