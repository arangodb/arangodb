/*=============================================================================
    Copyright (c) 2017 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/fusion/container/vector/vector.hpp>
#include <boost/type.hpp>

int main()
{
    boost::fusion::vector<int, float> v1;
    boost::fusion::vector<int, float> v2(v1);
    v1 = v2;
}
