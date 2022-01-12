/* boost random_test.cpp various tests
 *
 * Copyright (c) 2010 Steven Watanabe
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENCE_1_0.txt)
 *
 * $Id$
 */

#include <boost/random/random_device.hpp>

#include <boost/test/test_tools.hpp>
#include <boost/test/included/test_exec_monitor.hpp>

#ifndef BOOST_NO_CXX11_CONSTEXPR
constexpr boost::random_device::result_type const_min = (boost::random_device::min)();
constexpr boost::random_device::result_type const_max = (boost::random_device::max)();
#endif

int test_main(int, char**) {
    boost::random_device rng;
    double entropy = rng.entropy();
    BOOST_CHECK_GE(entropy, 0);
    for(int i = 0; i < 100; ++i) {
        boost::random_device::result_type val = rng();
        BOOST_CHECK_GE(val, (rng.min)());
        BOOST_CHECK_LE(val, (rng.max)());
    }

    boost::uint32_t a[10];
    rng.generate(a, a + 10);
    return 0;
}
