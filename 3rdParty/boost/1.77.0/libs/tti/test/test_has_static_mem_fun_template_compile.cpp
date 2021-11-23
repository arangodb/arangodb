
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include "test_has_static_mem_fun_template.hpp"
#include <boost/mpl/assert.hpp>

int main()
  {
  
  // You can always instantiate without compiler errors
  
  BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE_GEN(StatFuncTemplate)<AType::AStructType,bool,boost::mpl::vector<short *,long> > aVar;
  TAnother<AType,void,boost::mpl::vector<int,long &,const long &>,boost::function_types::const_qualified> aVar2;
  BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE_GEN(YetAnotherFuncTemplate)<AnotherType,void,boost::mpl::vector<const int &,float &> > aVar3;
  
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE_GEN(StatFuncTemplate)<AType::AStructType,int,boost::mpl::vector<short *,long> >));
  BOOST_MPL_ASSERT((TAnother<AType,void,boost::mpl::vector<int,long &,const long &> >));
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE_GEN(YetAnotherFuncTemplate)<AnotherType,void,boost::mpl::vector<const double &,float &> >));
  
  return 0;

  }
