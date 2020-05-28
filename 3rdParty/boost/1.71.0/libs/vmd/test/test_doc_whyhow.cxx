
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/elem.hpp>
#include <boost/vmd/equal.hpp>
#include <boost/vmd/get_type.hpp>
#include <boost/vmd/is_identifier.hpp>
#include <boost/vmd/is_seq.hpp>
#include <boost/vmd/is_tuple.hpp>
#include <boost/vmd/to_tuple.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

 #define BOOST_VMD_REGISTER_Seq (Seq)
 #define BOOST_VMD_REGISTER_Tuple (Tuple)
 #define BOOST_VMD_REGISTER_Unknown (Unknown)

 #define BOOST_VMD_DETECT_Seq_Seq
 #define BOOST_VMD_DETECT_Tuple_Tuple
 #define BOOST_VMD_DETECT_Unknown_Unknown

 #define AMACRO(param)                 \
   BOOST_PP_IIF                        \
     (                                 \
     BOOST_VMD_IS_SEQ(param),          \
     Seq,                              \
     BOOST_PP_IIF                      \
       (                               \
       BOOST_VMD_IS_TUPLE(param),      \
       Tuple,                          \
       Unknown                         \
       )                               \
     )
     
 BOOST_TEST(BOOST_VMD_EQUAL(AMACRO((0)(1)),Seq));
 BOOST_TEST(BOOST_VMD_EQUAL(AMACRO((0,1)),Tuple));
 BOOST_TEST(BOOST_VMD_EQUAL(AMACRO(24),Unknown));
     
 #define BOOST_VMD_REGISTER_NAME (NAME)
 #define BOOST_VMD_REGISTER_ADDRESS (ADDRESS)
 
 #define AMACRO1(param)              \
   BOOST_PP_IIF                      \
     (                               \
     BOOST_VMD_IS_IDENTIFIER(param), \
     AMACRO1_IDENTIFIER,             \
     AMACRO1_NO_IDENTIFIER           \
     )                               \
   (param)
   
 #define AMACRO1_IDENTIFIER(param) AMACRO1_ ## param
 #define AMACRO1_NO_IDENTIFIER(param) Parameter is not an identifier
 #define AMACRO1_NAME Identifier is a NAME
 #define AMACRO1_ADDRESS Identifier is an ADDRESS
 
 #define BOOST_VMD_REGISTER_Parameter (Parameter)
 #define BOOST_VMD_REGISTER_is (is)
 #define BOOST_VMD_REGISTER_not (not)
 #define BOOST_VMD_REGISTER_an (an)
 #define BOOST_VMD_REGISTER_identifier (identifier)
 #define BOOST_VMD_REGISTER_Identifier (Identifier)
 #define BOOST_VMD_REGISTER_a (a)
 
 #define BOOST_VMD_DETECT_NAME_NAME
 #define BOOST_VMD_DETECT_ADDRESS_ADDRESS
 #define BOOST_VMD_DETECT_Parameter_Parameter
 #define BOOST_VMD_DETECT_is_is
 #define BOOST_VMD_DETECT_not_not
 #define BOOST_VMD_DETECT_an_an
 #define BOOST_VMD_DETECT_identifier_identifier
 #define BOOST_VMD_DETECT_Identifier_Identifier
 #define BOOST_VMD_DETECT_a_a
 
 BOOST_TEST(BOOST_VMD_EQUAL(AMACRO1((44)),Parameter is not an identifier));
 BOOST_TEST(BOOST_VMD_EQUAL(AMACRO1(NAME),Identifier is a NAME));
 BOOST_TEST(BOOST_VMD_EQUAL(AMACRO1(ADDRESS),Identifier is an ADDRESS));
 
 #define AMACRO2(param)                        \
   BOOST_PP_IIF                                \
     (                                         \
     BOOST_VMD_IS_IDENTIFIER(param,NAME),      \
     AMACRO2_NAME,                             \
     BOOST_PP_IIF                              \
       (                                       \
       BOOST_VMD_IS_IDENTIFIER(param,ADDRESS), \
       AMACRO2_ADDRESS,                        \
       AMACRO2_NO_IDENTIFIER                   \
       )                                       \
     )                                         \
   (param)
   
 #define BOOST_VMD_REGISTER_or (or)
 #define BOOST_VMD_DETECT_or_or
 
 #define AMACRO2_NO_IDENTIFIER(param) Parameter is not a NAME or ADDRESS identifier
 #define AMACRO2_NAME(param) Identifier is a NAME
 #define AMACRO2_ADDRESS(param) Identifier is an ADDRESS
 
 BOOST_TEST(BOOST_VMD_EQUAL(AMACRO2((44)),Parameter is not a NAME or ADDRESS identifier));
 BOOST_TEST(BOOST_VMD_EQUAL(AMACRO2(NAME),Identifier is a NAME));
 BOOST_TEST(BOOST_VMD_EQUAL(AMACRO2(ADDRESS),Identifier is an ADDRESS));
 
 #define ASEQUENCE (1,2) NAME 147 BOOST_VMD_TYPE_NUMBER (a)(b)
 
 #define BOOST_VMD_REGISTER_b (b)
 #define BOOST_VMD_DETECT_b_b
 
 BOOST_TEST(BOOST_VMD_EQUAL(BOOST_VMD_TO_TUPLE(ASEQUENCE),((1,2),NAME,147,BOOST_VMD_TYPE_NUMBER,(a)(b))));
 BOOST_TEST(BOOST_VMD_EQUAL(BOOST_VMD_ELEM(2,ASEQUENCE),147));
 
 BOOST_TEST(BOOST_VMD_EQUAL(BOOST_VMD_GET_TYPE((1,2)),BOOST_VMD_TYPE_TUPLE));
 BOOST_TEST(BOOST_VMD_EQUAL(BOOST_VMD_GET_TYPE(235),BOOST_VMD_TYPE_NUMBER));
 
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
