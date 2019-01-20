/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    issue8.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include "test.hpp"
#include <vector>
#include <boost/hof/pipable.hpp>
#include <boost/hof/placeholders.hpp>
#include <algorithm>


struct filter_fn
{
    template<class Input, class P>
    Input operator()(Input input, P pred) const
    {
        Input output(input.size());
        output.erase(
            ::std::copy_if(
                ::std::begin(input),
                ::std::end(input),
                ::std::begin(output),
                pred
            ),
            ::std::end(output)
        );
        return output;
    }
};

static constexpr boost::hof::pipable_adaptor<filter_fn> filter = {};

BOOST_HOF_TEST_CASE()
{
    std::vector<int> data;
    for(int i=0;i<6;i++)
    {
        data.push_back(i);
    }
    std::vector<int> r1 = data | filter(boost::hof::_1 > 1);
    BOOST_HOF_TEST_CHECK(r1.size() == 4);

    std::vector<int> r2 = filter(data, boost::hof::_1 > 1);
    BOOST_HOF_TEST_CHECK(r2.size() == 4);
}
