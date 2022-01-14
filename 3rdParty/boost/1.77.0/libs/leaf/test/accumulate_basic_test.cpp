// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/on_error.hpp>
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

leaf::error_id g()
{
    auto load = leaf::on_error( [](info<1> & x) {++x.value;} );
    return leaf::new_error();
}

leaf::error_id f()
{
    auto load = leaf::on_error( [](info<1> & x) {++x.value;}, [](info<2> & x) {++x.value;} );
    return g();
}

int main()
{
    int r = leaf::try_handle_all(
        []() -> leaf::result<int>
        {
            return f();
        },
        []( info<1> const & a, info<2> const & b )
        {
            BOOST_TEST_EQ(a.value, 2);
            BOOST_TEST_EQ(b.value, 1);
            return 1;
        },
        []
        {
            return 2;
        } );
    BOOST_TEST_EQ(r, 1);
    return boost::report_errors();
}
