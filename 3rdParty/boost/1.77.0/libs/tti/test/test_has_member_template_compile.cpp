
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include "test_has_mem_fun_template.hpp"
#include <boost/mpl/assert.hpp>

int main()
  {
  
  // You can always instantiate without compiler errors
  
  BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(someFunctionMemberTemplate)<int (AnotherType::*)(float,long,bool)> aVar;
  
  // Compile time asserts
  
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(SomeFuncTemplate)<double (AType::BType::CType::*)(int,long *,double &)>));
  BOOST_MPL_ASSERT((SameName<void (AType::*)(int *,int,float &)>));
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(AFuncTemplate)<int (AType::*)(const long &)>));
  BOOST_MPL_ASSERT((AnotherName<long (AnotherType::*)(bool &)>));
  
  return 0;

  }
