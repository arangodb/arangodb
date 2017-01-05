
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/size.hpp>
#include <boost/detail/lightweight_test.hpp>

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
  
  /* SIZE */
  
  BOOST_TEST_EQ(BOOST_VMD_SIZE(ASEQUENCE),6);
  BOOST_TEST_EQ(BOOST_VMD_SIZE(ASEQUENCE2),5);
  BOOST_TEST_EQ(BOOST_VMD_SIZE(ASEQUENCE3),3);
  BOOST_TEST_EQ(BOOST_VMD_SIZE(ASEQUENCE4),0);
  
#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
