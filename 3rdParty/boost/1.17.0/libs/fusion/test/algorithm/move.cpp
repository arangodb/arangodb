/*=============================================================================
    Copyright (c) 2014,2018 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/core/lightweight_test.hpp>
#include <boost/fusion/container/vector/vector.hpp>
#include <boost/fusion/container/list/list.hpp>
#include <boost/fusion/sequence/comparison.hpp>
#include <boost/fusion/algorithm/auxiliary/move.hpp>
#include <utility>

int main()
{
    {
        boost::fusion::vector<int, short, double> v(1, 2, 3);
        boost::fusion::list<int, short, double> l1 = v;
        boost::fusion::list<int, short, double> l2;

        boost::fusion::move(std::move(v), l2);
        BOOST_TEST(l1 == l2);
    }

    return boost::report_errors();
}

