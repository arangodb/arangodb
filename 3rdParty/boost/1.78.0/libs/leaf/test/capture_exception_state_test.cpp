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
#   include <boost/leaf/capture.hpp>
#   include <boost/leaf/handle_errors.hpp>
#   include <boost/leaf/exception.hpp>
#endif

#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

int count = 0;

template <int>
struct info
{
    info() noexcept
    {
        ++count;
    }

    info( info const & ) noexcept
    {
        ++count;
    }

    ~info() noexcept
    {
        --count;
    }
};

int main()
{
    auto error_handlers = std::make_tuple(
        []( info<1>, info<3> )
        {
            return 1;
        },
        []
        {
            return 2;
        } );
    BOOST_TEST_EQ(count, 0);
    std::exception_ptr ep;
    try
    {
        leaf::capture(
            leaf::make_shared_context(error_handlers),
            []
            {
                throw leaf::exception(info<1>{}, info<3>{});
            } );
        BOOST_TEST(false);
    }
    catch(...)
    {
        ep = std::current_exception();
    }
    BOOST_TEST_EQ(count, 2);
    int r = leaf::try_catch(
        [&]() -> int
        {
            std::rethrow_exception(ep);
        },
        error_handlers );
    BOOST_TEST_EQ(r, 1);
    ep = std::exception_ptr();
    BOOST_TEST_EQ(count, 0);
    return boost::report_errors();
}

#endif
