/*=============================================================================
    Copyright (c) 2005-2007 Dan Marsden
    Copyright (c) 2005-2007 Joel de Guzman
    Copyright (c) 2014      John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/phoenix.hpp>
#include <boost/range/as_literal.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::phoenix::placeholders;
using namespace boost::phoenix;
    
int main()
{
  char X('x');
    find(boost::as_literal("fox"), 'x')();     // works
#if !(defined (BOOST_NO_CXX11_DECLTYPE) || \
      defined (BOOST_INTEL_CXX_VERSION) || \
      (BOOST_GCC_VERSION < 40500) )
    const char *Y = find(boost::as_literal("fox"), arg1)('x'); // works for C++11
#else
    const char *Y = find(boost::as_literal("fox"), construct<char>(arg1))('x'); // works
#endif
    BOOST_TEST(X == *Y);

    return boost::report_errors();
}
