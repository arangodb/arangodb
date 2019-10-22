
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/equal.hpp>
#include <boost/vmd/identity.hpp>
#include <libs/vmd/test/test_doc_example_switch.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

//[ example_switch_defines

#define BOOST_VMD_SWITCH_TEST_1(number) \
    test1_ ## number
/**/

#define BOOST_VMD_SWITCH_TEST_2(number) \
    test2_ ## number
/**/

#define BOOST_VMD_SWITCH_TEST_3(number) \
    test3_ ## number
/**/

#define BOOST_VMD_SWITCH_TEST_DEFAULT(number) \
    test_default_ ## number
/**/

//]

#define BOOST_VMD_REGISTER_test1_7 (test1_7)
#define BOOST_VMD_REGISTER_test2_7 (test2_7)
#define BOOST_VMD_REGISTER_test3_7 (test3_7)
#define BOOST_VMD_REGISTER_test_default_7 (test_default_7)
#define BOOST_VMD_DETECT_test1_7_test1_7
#define BOOST_VMD_DETECT_test2_7_test2_7
#define BOOST_VMD_DETECT_test3_7_test3_7
#define BOOST_VMD_DETECT_test_default_7_test_default_7

BOOST_TEST(BOOST_VMD_EQUAL
            (
//[ example_switch_defines_t1
            BOOST_VMD_SWITCH(1,
                            (7),
                            (BOOST_VMD_SWITCH_TEST_DEFAULT),
                            (3,BOOST_VMD_SWITCH_TEST_3),
                            (1,BOOST_VMD_SWITCH_TEST_1),
                            (2,BOOST_VMD_SWITCH_TEST_2)
                            )
//]
            ,test1_7
            )
          );

BOOST_TEST(BOOST_VMD_EQUAL
            (
            BOOST_VMD_SWITCH(2,
                            (7),
                            (BOOST_VMD_SWITCH_TEST_DEFAULT),
                            (1,BOOST_VMD_SWITCH_TEST_1),
                            (3,BOOST_VMD_SWITCH_TEST_3),
                            (2,BOOST_VMD_SWITCH_TEST_2)
                            )
            ,test2_7
            )
          );

BOOST_TEST(BOOST_VMD_EQUAL
            (
            BOOST_VMD_SWITCH(3,
                            (7),
                            (BOOST_VMD_SWITCH_TEST_DEFAULT),
                            (1,BOOST_VMD_SWITCH_TEST_1),
                            (2,BOOST_VMD_SWITCH_TEST_2),
                            (3,BOOST_VMD_SWITCH_TEST_3)
                            )
            ,test3_7
            )
          );
          
BOOST_TEST(BOOST_VMD_EQUAL
            (
//[ example_switch_defines_t4
            BOOST_VMD_SWITCH(4,
                            (7),
                            (BOOST_VMD_SWITCH_TEST_DEFAULT),
                            (2,BOOST_VMD_SWITCH_TEST_2),
                            (1,BOOST_VMD_SWITCH_TEST_1),
                            (3,BOOST_VMD_SWITCH_TEST_3)
                            )
//]
            ,test_default_7
            )
          );
          
BOOST_TEST(BOOST_VMD_EQUAL
            (
//[ example_switch_defines_t5
            BOOST_VMD_SWITCH(143,
                            (7),
                            (BOOST_VMD_SWITCH_TEST_DEFAULT),
                            (1,BOOST_VMD_SWITCH_TEST_1),
                            (2,BOOST_VMD_SWITCH_TEST_2),
                            (3,BOOST_VMD_SWITCH_TEST_3),
                            (143,BOOST_VMD_SWITCH_IDENTITY(55))
                            )
//]
            ,55
            )
          );
          
BOOST_TEST(BOOST_VMD_EQUAL
            (
//[ example_switch_defines_t6
            BOOST_VMD_SWITCH(155,
                            (7),
                            (BOOST_VMD_SWITCH_IDENTITY(77)),
                            (1,BOOST_VMD_SWITCH_TEST_1),
                            (2,BOOST_VMD_SWITCH_TEST_2),
                            (3,BOOST_VMD_SWITCH_TEST_3),
                            (143,BOOST_VMD_SWITCH_IDENTITY(55))
                            )
//]
            ,77
            )
          );
          
BOOST_TEST(BOOST_VMD_EQUAL
            (
//[ example_switch_defines_t7
            BOOST_VMD_SWITCH(BOOST_VMD_TYPE_TUPLE,
                            (7),
                            (BOOST_VMD_SWITCH_TEST_DEFAULT),
                            (BOOST_VMD_TYPE_TUPLE,BOOST_VMD_SWITCH_TEST_1),
                            ((1,2,3),BOOST_VMD_SWITCH_TEST_3),
                            (2,BOOST_VMD_SWITCH_TEST_2)
                            )
//]
            ,test1_7
            )
          );

#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
