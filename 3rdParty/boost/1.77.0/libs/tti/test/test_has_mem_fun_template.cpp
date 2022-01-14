
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include "test_has_mem_fun_template.hpp"
#include <boost/detail/lightweight_test.hpp>

int main()
  {
  
  BOOST_TEST((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(SomeFuncTemplate)<AType::BType::CType,double,boost::mpl::vector<int,long *,double &> >::value));
  BOOST_TEST((SameName<AType,void,boost::mpl::vector<int *,int,float &> >::value));
  BOOST_TEST((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(AFuncTemplate)<AType,int,boost::mpl::vector<const long &> >::value));
  BOOST_TEST((AnotherName<AnotherType,long,boost::mpl::vector<bool &> >::value));
  
  // Wrong enclosing type
  
  BOOST_TEST((!BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(SomeFuncTemplate)<AType::BType,double,boost::mpl::vector<int,long *,double &> >::value));
  
  // Wrong return value
  
  BOOST_TEST((!SameName<AType,int,boost::mpl::vector<int *,int,float &> >::value));
  
  // Mismatched paramaters
  
  BOOST_TEST((!BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(AFuncTemplate)<AType,int,boost::mpl::vector<const bool &> >::value));
  
  // Invalid enclosing type
  
  BOOST_TEST((!AnotherName<int,long,boost::mpl::vector<bool &> >::value));
  
  return boost::report_errors();

  }
