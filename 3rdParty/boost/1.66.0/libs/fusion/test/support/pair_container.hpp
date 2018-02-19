/*=============================================================================
    Copyright (c) 2014 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/core/ignore_unused.hpp>
#include <boost/fusion/support/pair.hpp>

using namespace boost::fusion;

template <typename C>
void copy()
{
    pair<int, C> src;
    pair<int, C> dest = src;
    boost::ignore_unused(dest);
}

void test()
{
    copy<FUSION_SEQUENCE<> >();
    copy<FUSION_SEQUENCE<TEST_TYPE> >();
    copy<FUSION_SEQUENCE<TEST_TYPE, TEST_TYPE> >();
}

