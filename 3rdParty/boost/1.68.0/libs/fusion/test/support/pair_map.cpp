/*=============================================================================
    Copyright (c) 2014 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/container/map/map.hpp>

#define FUSION_SEQUENCE map
#define TEST_TYPE pair<int,int>
#include "./pair_container.hpp"

int main()
{
    test();
}

