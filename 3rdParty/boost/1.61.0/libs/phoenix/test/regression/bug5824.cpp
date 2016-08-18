/*=============================================================================
    Copyright (c) 2005-2007 Dan Marsden
    Copyright (c) 2005-2007 Joel de Guzman
    Copyright (c) 2014      John Fletcher
    
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

// Check for Bug5824

#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>

#include <boost/detail/lightweight_test.hpp>

using namespace boost::phoenix::arg_names;

int main()
{
  int a = 0;
  (++arg1, ++arg1)(a);
  BOOST_TEST(a == 2);
}
