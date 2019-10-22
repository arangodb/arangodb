/*=============================================================================
    Copyright (c) 2014      John Fletcher
    
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/phoenix.hpp>
#include <vector>

#include <boost/detail/lightweight_test.hpp>

namespace phx = boost::phoenix;

int main()
{
    std::vector<int> v = phx::let(phx::local_names::_a = std::vector<int>(3))
    [
       phx::local_names::_a
    ]
    ();
    BOOST_TEST( v.size() == 3);
}
