/*=============================================================================
    Copyright (c) 2005-2007 Dan Marsden
    Copyright (c) 2005-2007 Joel de Guzman
    Copyright (c) 2014      John Fletcher
    
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/fusion/sequence/comparison.hpp>
#include <boost/fusion/sequence/sequence_facade.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator/comparison.hpp>

struct foo : boost::fusion::sequence_facade<foo, boost::fusion::random_access_traversal_tag>
{
    // Rest of the sequence_facade extension mechanism code omitted,
    // as it is not needed to reproduce the error
 
    foo() : x(), y() {}
    
    int x;
    int y;
};
 
int main()
{
  /*auto x = */ boost::phoenix::arg_names::arg1 < foo();
}
