// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/detail/config.hpp>
#ifdef BOOST_LEAF_NO_EXCEPTIONS

#include <iostream>

int main()
{
    std::cout << "Unit test not applicable." << std::endl;
    return 0;
}

#else

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/handle_errors.hpp>
#   include <boost/leaf/exception.hpp>
#   include <boost/leaf/pred.hpp>
#endif

#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

struct info { int value; };

struct my_error: std::exception { };

int main()
{
    int r = leaf::try_catch(
        []() -> int
        {
            throw leaf::exception( my_error(), info{42} );
        },
        []( my_error const & x, leaf::catch_<leaf::error_id> id )
        {
            BOOST_TEST(dynamic_cast<leaf::error_id const *>(&id.matched)!=0 && dynamic_cast<leaf::error_id const *>(&id.matched)->value()==1);
            return 1;
        },
        []
        {
            return 2;
        } );
    BOOST_TEST_EQ(r, 1);
    return boost::report_errors();
}

#endif
