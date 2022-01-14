// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/common.hpp>
#   include <boost/leaf/handle_errors.hpp>
#   include <boost/leaf/result.hpp>
#endif

#include "lightweight_test.hpp"
#include <sstream>

namespace leaf = boost::leaf;

int main()
{
    errno = ENOENT;
    std::stringstream ss;
    ss << leaf::e_errno{};
    BOOST_TEST(ss.str().find(std::strerror(ENOENT)) != std::string::npos);

    int r = leaf::try_handle_all(
        []() -> leaf::result<int>
        {
            struct reset_errno { ~reset_errno() { errno=0; } } reset;
            return leaf::new_error( leaf::e_errno{} );
        },
        []( leaf::e_errno e )
        {
            BOOST_TEST_EQ(errno, 0);
            BOOST_TEST_EQ(e.value, ENOENT);
            return 1;
        },
        []
        {
            return 2;
        } );
    BOOST_TEST_EQ(r, 1);
    return boost::report_errors();
}
