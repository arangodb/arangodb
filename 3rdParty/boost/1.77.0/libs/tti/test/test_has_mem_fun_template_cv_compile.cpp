
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include "test_has_mem_fun_template.hpp"
#include <boost/mpl/assert.hpp>

int main()
  {
  
  // Use const enclosing type
  
  BOOST_MPL_ASSERT((FirstCMFT<const AType,double,boost::mpl::vector<short,long> >));
  
  // Use const_qualified
  
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(WFunctionTmp)<AType,void,boost::mpl::vector<int **, long &, bool>,boost::function_types::const_qualified>));
  
  // Use volatile enclosing type
  
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(AVolatileFT)<volatile AType,double,boost::mpl::vector<float, long, char> >));
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(VWithDefault)<volatile AnotherType,void,boost::mpl::vector<float,double> >));
  
  // Use volatile_qualified
  
  BOOST_MPL_ASSERT((VolG<AType,void,boost::mpl::vector<long &>,boost::function_types::volatile_qualified>));
  
  // Use const volatile enclosing type
  
  BOOST_MPL_ASSERT((ACV<const volatile AType,double,boost::mpl::vector<int,short> >));
  
  // Use const_volatile_qualified
  
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_MEMBER_FUNCTION_TEMPLATE_GEN(ConstVolTTFun)<AType,void,boost::mpl::vector<float,double>,boost::function_types::cv_qualified>));
  
  return 0;

  }
