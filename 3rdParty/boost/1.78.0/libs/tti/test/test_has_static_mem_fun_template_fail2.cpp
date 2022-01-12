
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include "test_has_static_mem_fun_template.hpp"

int main()
  {
  
  // Function signature has type which does not exist
  
  BOOST_TTI_HAS_STATIC_MEMBER_FUNCTION_TEMPLATE_GEN(YetAnotherFuncTemplate)<AnotherType,AClassType,boost::mpl::vector<const double &,float &> > aVar;
  
  return 0;

  }
