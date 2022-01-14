// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/capture.hpp>
#   include <boost/leaf/result.hpp>
#   include <boost/leaf/handle_errors.hpp>
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
            return 42;
        },
        []
        {
            return -42;
        } );

    {
        auto r = leaf::capture(
            leaf::make_shared_context(error_handlers),
            []() -> leaf::result<int>
            {
                return leaf::new_error( info<1>{}, info<3>{} );
            } );
        BOOST_TEST_EQ(count, 2);

        int answer = leaf::try_handle_all(
            [&r]
            {
                return std::move(r);
            },
            error_handlers);
        BOOST_TEST_EQ(answer, 42);
    }
    BOOST_TEST_EQ(count, 0);

    return boost::report_errors();
}
