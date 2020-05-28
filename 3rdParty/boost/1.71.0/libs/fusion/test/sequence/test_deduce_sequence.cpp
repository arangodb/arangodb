/*=============================================================================
    Copyright (c) 2009 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/mpl/vector.hpp>
#include <boost/fusion/support.hpp>

typedef boost::fusion::traits::deduce_sequence < 

boost::mpl::vector<int, char> 

>::type seq1_t;


typedef boost::fusion::traits::deduce_sequence < 

boost::fusion::vector<int, char> 

>::type seq2_t;
