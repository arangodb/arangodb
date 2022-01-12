
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#include "test_has_fun_template.hpp"
#include <boost/mpl/assert.hpp>

int main()
  {
  
  // Wrong function signature for AFuncTemplate
  
  BOOST_MPL_ASSERT((BOOST_TTI_HAS_FUNCTION_TEMPLATE_GEN(AFuncTemplate)<AType,void,boost::mpl::vector<double *,unsigned char,short &> >));
  
  return 0;

  }
