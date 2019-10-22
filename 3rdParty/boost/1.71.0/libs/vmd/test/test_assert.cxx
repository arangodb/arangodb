
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/elem.hpp>
#include <boost/vmd/assert.hpp>
#include <boost/vmd/is_identifier.hpp>
#include <boost/vmd/is_empty.hpp>
#include <boost/vmd/is_number.hpp>
#include <boost/vmd/is_type.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/preprocessor/arithmetic/dec.hpp>
#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/comparison/equal.hpp>
#include <boost/preprocessor/facilities/empty.hpp>
#include <boost/preprocessor/list/at.hpp>
#include <boost/preprocessor/logical/compl.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/tuple/elem.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define A_PTUPLE (*,#,44)
  #define AN_ARRAY_PLUS (4,(mmf,34,^^,!)) 156
  #define A_TUPLE (*,#,zzz ())
  #define DATA
  #define JDATA somevalue
  #define KDATA (a,b) name
  #define A_LIST (eeb,(grist,(152,BOOST_PP_NIL)))
  #define A_LIST_PLUS (mmf,(34,(^^,(!,BOOST_PP_NIL)))) 56
  #define A_SEQ (73 (split) clear)(red)(green 44)
  #define A_SEQ2 (73 (split) clear)(BOOST_VMD_TYPE_LIST)(green 44)
  #define A_SEQ_PLUS (mmf)(34)(^^)(!) 456
  
  #define BOOST_VMD_REGISTER_zzz (zzz)
  #define BOOST_VMD_DETECT_zzz_zzz
  #define BOOST_VMD_REGISTER_somevalue (somevalue)
  #define BOOST_VMD_DETECT_somevalue_somevalue
  
  BOOST_VMD_ASSERT(25)
  BOOST_VMD_ASSERT(BOOST_PP_DEC(2))
  BOOST_VMD_ASSERT(BOOST_PP_INC(255))
  BOOST_VMD_ASSERT
      (
      BOOST_PP_EQUAL
          (
          BOOST_PP_TUPLE_ELEM(2,A_PTUPLE),
          44
          )
      )
      
  BOOST_VMD_ASSERT(BOOST_PP_EQUAL(BOOST_PP_TUPLE_ELEM(1,BOOST_VMD_ELEM(0,AN_ARRAY_PLUS,BOOST_VMD_RETURN_AFTER,BOOST_VMD_TYPE_ARRAY)),156))
  BOOST_VMD_ASSERT
      (
      BOOST_PP_EQUAL
          (
        BOOST_PP_TUPLE_ELEM
            (
            2,
            BOOST_VMD_ELEM(0,BOOST_PP_TUPLE_ELEM(2,A_TUPLE),(dummy1,zzz),BOOST_VMD_RETURN_AFTER,BOOST_VMD_RETURN_INDEX,BOOST_VMD_TYPE_IDENTIFIER)
            ),
        1
          )
      )
      
  BOOST_VMD_ASSERT(BOOST_VMD_IS_EMPTY(DATA BOOST_PP_EMPTY()))
  BOOST_VMD_ASSERT(BOOST_VMD_IS_IDENTIFIER(JDATA,somevalue))
  BOOST_VMD_ASSERT(BOOST_VMD_IS_NUMBER(BOOST_PP_LIST_AT(A_LIST,2)))
  BOOST_VMD_ASSERT(BOOST_PP_EQUAL(BOOST_PP_TUPLE_ELEM(1,BOOST_VMD_ELEM(0,A_LIST_PLUS,BOOST_VMD_RETURN_AFTER,BOOST_VMD_TYPE_LIST)),56))
  BOOST_VMD_ASSERT(BOOST_PP_EQUAL(BOOST_PP_TUPLE_ELEM(0,BOOST_VMD_ELEM(0,BOOST_PP_SEQ_ELEM(0,A_SEQ),BOOST_VMD_RETURN_AFTER,BOOST_VMD_TYPE_NUMBER)),73))
  BOOST_VMD_ASSERT(BOOST_PP_EQUAL(BOOST_PP_SEQ_ELEM(1,BOOST_PP_TUPLE_ELEM(0,BOOST_VMD_ELEM(0,A_SEQ_PLUS,BOOST_VMD_RETURN_AFTER,BOOST_VMD_TYPE_SEQ))),34))
  BOOST_VMD_ASSERT(BOOST_PP_COMPL(BOOST_VMD_IS_EMPTY(BOOST_PP_TUPLE_ELEM(1,BOOST_VMD_ELEM(0,KDATA,BOOST_VMD_RETURN_AFTER,BOOST_VMD_TYPE_TUPLE)))))
  
  BOOST_VMD_ASSERT(BOOST_VMD_IS_TYPE(BOOST_PP_SEQ_ELEM(1,A_SEQ2)))
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
