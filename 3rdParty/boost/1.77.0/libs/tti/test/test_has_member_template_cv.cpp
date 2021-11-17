
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include "test_has_mem_fun_template.hpp"
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
  // Use const
  
  BOOST_TEST((FirstCMFT<double (AType::*)(short,long) const>::value));
  BOOST_TEST((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(WFunctionTmp)<void (AType::*)(int **, long &, bool) const>::value));
  
  // Use volatile
  
  BOOST_TEST((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(AVolatileFT)<double (AType::*)(float, long, char) volatile>::value));
  BOOST_TEST((VolG<void (AType::*)(long &) volatile>::value));
  
  // Use const volatile
  
  BOOST_TEST((ACV<double (AType::*)(int,short) const volatile>::value));
  BOOST_TEST((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(ConstVolTTFun)<void (AType::*)(float,double) const volatile>::value));
  
  return boost::report_errors();

  }
