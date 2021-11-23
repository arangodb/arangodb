// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/handle_errors.hpp>
#include <boost/leaf/pred.hpp>
#include <boost/leaf/result.hpp>

namespace leaf = boost::leaf;

int main()
{
    return leaf::try_handle_all(
        []() -> leaf::result<int>
        {
            return 0;
        },
        []( leaf::catch_<int> const & ) // leaf::catch_<> must be taken by value
        {
            return 1;
        },
        []
        {
            return 2;
        });
    return 0;
}
