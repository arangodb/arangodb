// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/handle_errors.hpp>
#include <boost/exception/info.hpp>

namespace leaf = boost::leaf;

struct test_ex: std::exception { };

typedef boost::error_info<struct test_info_, int> test_info;

int main()
{
    leaf::try_catch(
        []
        {
        },
        []( test_info const & x ) // boost::error_info must be taken by value
        {
        } );

    return 0;
}
