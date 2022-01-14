
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include "test_has_mem_fun_template.hpp"
#include <boost/mpl/assert.hpp>

int main()
  {
  
  // Use const
  
  BOOST_MPL_ASSERT((FirstCMFT<double (AType::*)(short,long) const>));
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(WFunctionTmp)<void (AType::*)(int **, long &, bool) const>));
  
  // Use volatile
  
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(AVolatileFT)<double (AType::*)(float, long, char) volatile>));
  BOOST_MPL_ASSERT((VolG<void (AType::*)(long &) volatile>));
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(VWithDefault)<void (AnotherType::*)(float, double) volatile>));
  
  // Use const volatile
  
  BOOST_MPL_ASSERT((ACV<double (AType::*)(int,short) const volatile>));
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(ConstVolTTFun)<void (AType::*)(float,double) const volatile>));
  
  return 0;

  }
