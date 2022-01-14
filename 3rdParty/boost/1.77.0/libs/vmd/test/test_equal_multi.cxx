
//  (C) Copyright Edward Diener 2011-2015
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include <boost/vmd/equal.hpp>
#include <boost/vmd/not_equal.hpp>
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
#if BOOST_PP_VARIADICS

  #define BOOST_VMD_REGISTER_ggh (ggh)
  #define BOOST_VMD_REGISTER_dvd (dvd)
  #define BOOST_VMD_REGISTER_aaa (aaa)
  #define BOOST_VMD_REGISTER_bbb (bbb)
  #define BOOST_VMD_REGISTER_ccc (ccc)
  #define BOOST_VMD_REGISTER_ddd (ddd)
  #define BOOST_VMD_REGISTER_eee (eee)
  #define BOOST_VMD_REGISTER_fff (fff)
  #define BOOST_VMD_REGISTER_ggg (ggg)
  #define BOOST_VMD_REGISTER_hhh (hhh)
  #define BOOST_VMD_DETECT_ggh_ggh
  #define BOOST_VMD_DETECT_dvd_dvd
  #define BOOST_VMD_DETECT_aaa_aaa
  #define BOOST_VMD_DETECT_bbb_bbb
  #define BOOST_VMD_DETECT_ccc_ccc
  #define BOOST_VMD_DETECT_ddd_ddd
  #define BOOST_VMD_DETECT_eee_eee
  #define BOOST_VMD_DETECT_fff_fff
  #define BOOST_VMD_DETECT_ggg_ggg
  #define BOOST_VMD_DETECT_hhh_hhh
  
  #define ANIDENTIFIER ggh
  #define ANIDENTIFIER2 dvd
  #define ANUMBER 249
  #define ANUMBER2 17
  #define ASEQ (25)(26)((27)(28) ggh)
  #define ATUPLE (0,1,2,3,((aaa,bbb))((ccc,ddd))((eee))((fff,ggg,hhh)))
  #define ALIST (0,(1,(2,(3,BOOST_PP_NIL))))
  #define ANARRAY (3,(a,b,38))
  #define ASEQUENCE ANUMBER ALIST ATUPLE ANIDENTIFIER ANARRAY ASEQ
  #define ASEQUENCE2 ANIDENTIFIER2 ASEQ ALIST ANUMBER ATUPLE
  #define ASEQUENCE3 ASEQ ANUMBER2 ATUPLE
  #define ASEQUENCE4
  #define ASEQUENCE5 ANIDENTIFIER2 ASEQ ALIST ANUMBER ATUPLE
  #define ASEQUENCE6 ASEQ ANUMBER2 ATUPLE
  
  BOOST_TEST(BOOST_VMD_EQUAL(ASEQUENCE2,ASEQUENCE5));
  BOOST_TEST(BOOST_VMD_EQUAL(ASEQUENCE3,ASEQUENCE6));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ASEQUENCE,ASEQUENCE2));
  BOOST_TEST(BOOST_VMD_NOT_EQUAL(ASEQUENCE3,ASEQUENCE4));

#else

BOOST_ERROR("No variadic macro support");
  
#endif

  return boost::report_errors();
  
  }
