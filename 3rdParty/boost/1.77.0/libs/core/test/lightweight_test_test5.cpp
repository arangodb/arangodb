//
// Test that BOOST_TEST_EQ doesn't emit sign compare warnings
//
// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/core/lightweight_test.hpp>

int main()
{
    BOOST_TEST_EQ(1, 1u);
    BOOST_TEST_EQ(~0u, -1);

    return boost::report_errors();
}
