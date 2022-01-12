
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/is_number.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/preprocessor/list/at.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/tuple/elem.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

#define A_TUPLE (4,241,zzz)
#define JDATA somevalue
#define KDATA 213
#define A_SEQ (num)(78)(42)
#define A_LIST (eeb,(grist,(152,BOOST_PP_NIL)))

#if BOOST_PP_LIMIT_MAG > 256

#define A_TUPLE_2 (374,511,zzz)
#define KDATA_2 269

#endif

#if BOOST_PP_LIMIT_MAG > 512

#define A_SEQ_2 (num)(781)(942)
#define A_LIST_2 (eeb,(grist,(1021,BOOST_PP_NIL)))

#endif

BOOST_TEST(!BOOST_VMD_IS_NUMBER(BOOST_PP_TUPLE_ELEM(2,A_TUPLE)));
BOOST_TEST(BOOST_VMD_IS_NUMBER(BOOST_PP_TUPLE_ELEM(1,A_TUPLE)));
BOOST_TEST(!BOOST_VMD_IS_NUMBER(JDATA));
BOOST_TEST(BOOST_VMD_IS_NUMBER(KDATA));
BOOST_TEST(!BOOST_VMD_IS_NUMBER(BOOST_PP_SEQ_ELEM(0,A_SEQ)));
BOOST_TEST(BOOST_VMD_IS_NUMBER(BOOST_PP_SEQ_ELEM(2,A_SEQ)));
BOOST_TEST(!BOOST_VMD_IS_NUMBER(BOOST_PP_LIST_AT(A_LIST,0)));
BOOST_TEST(BOOST_VMD_IS_NUMBER(BOOST_PP_LIST_AT(A_LIST,2)));
BOOST_TEST(!BOOST_VMD_IS_NUMBER(BOOST_PP_LIST_AT(A_LIST,1)));
BOOST_TEST(!BOOST_VMD_IS_NUMBER((XXX)));
BOOST_TEST(!BOOST_VMD_IS_NUMBER());
  
#if BOOST_PP_LIMIT_MAG > 256

BOOST_TEST(BOOST_VMD_IS_NUMBER(KDATA_2));
BOOST_TEST(BOOST_VMD_IS_NUMBER(BOOST_PP_TUPLE_ELEM(0,A_TUPLE_2)));

#endif

#if BOOST_PP_LIMIT_MAG > 512

BOOST_TEST(BOOST_VMD_IS_NUMBER(BOOST_PP_SEQ_ELEM(2,A_SEQ_2)));
BOOST_TEST(BOOST_VMD_IS_NUMBER(BOOST_PP_LIST_AT(A_LIST_2,2)));

#endif

#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
