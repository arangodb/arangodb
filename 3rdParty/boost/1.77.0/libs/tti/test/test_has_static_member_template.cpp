
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include "test_has_static_mem_fun_template.hpp"
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
  BOOST_TEST((BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE_GEN(StatFuncTemplate)<AType::AStructType,int(short *,long)>::value));
  BOOST_TEST((TAnother<AType,void (int,long &,const long &)>::value));
  BOOST_TEST((BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE_GEN(YetAnotherFuncTemplate)<AnotherType,void (const double &,float &)>::value));
  
  // Wrong enclosing type
  
  BOOST_TEST((!BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE_GEN(StatFuncTemplate)<AType,int (short *,long)>::value));
  
  // Wrong return value
  
  BOOST_TEST((!TAnother<AType,short (int,long &,const long &)>::value));
  
  // Mismatched paramaters
  
  BOOST_TEST((!BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE_GEN(YetAnotherFuncTemplate)<AnotherType,void (const long &,float &)>::value));
  
  return boost::report_errors();

  }
