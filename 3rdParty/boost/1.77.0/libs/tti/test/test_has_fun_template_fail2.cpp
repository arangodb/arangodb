
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include "test_has_fun_template.hpp"

int main()
  {
  
  // Function signature has type which does not exist
  
  BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(StatFuncTemplate)<NVType,int,boost::mpl::vector<int *,bool> > aVar;
  
  return 0;

  }
