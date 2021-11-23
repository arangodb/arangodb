// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/context.hpp>
#   include <boost/leaf/handle_errors.hpp>
#   include <boost/leaf/result.hpp>
#endif

#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

template <int>
struct info
{
    int value;
};

template <class Ctx>
leaf::result<int> f( Ctx & ctx )
{
    auto active_context = activate_context(ctx);
    return leaf::new_error( info<1>{1} );
}

int main()
{
    auto error_handlers = std::make_tuple(
        []( info<1> x )
        {
            BOOST_TEST(x.value==1);
            return 1;
        },
        []
        {
            return 2;
        } );

    int r = leaf::try_handle_all(
        [&]
        {
            auto ctx = leaf::make_context(error_handlers);
            auto active_context = activate_context(ctx);
            auto r = f(ctx);
            ctx.propagate();
            return r;
        },
        error_handlers );
    BOOST_TEST_EQ(r, 1);

    return boost::report_errors();
}
