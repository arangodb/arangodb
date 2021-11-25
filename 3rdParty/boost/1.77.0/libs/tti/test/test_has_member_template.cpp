
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include "test_has_mem_fun_template.hpp"
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
  BOOST_TEST((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(SomeFuncTemplate)<double (AType::BType::CType::*)(int,long *,double &)>::value));
  BOOST_TEST((SameName<void (AType::*)(int *,int,float &)>::value));
  BOOST_TEST((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(AFuncTemplate)<int (AType::*)(const long &)>::value));
  BOOST_TEST((AnotherName<long (AnotherType::*)(bool &)>::value));
  
  // Wrong enclosing type
  
  BOOST_TEST((!BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(SomeFuncTemplate)<double (AType::BType::*)(int,long *,double &)>::value));
  
  // Wrong return value
  
  BOOST_TEST((!SameName<int (AType::*)(int *,int,float &)>::value));
  
  // Mismatched paramaters
  
  BOOST_TEST((!BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(AFuncTemplate)<int (AType::*)(const bool &)>::value));
  
  return boost::report_errors();

  }
