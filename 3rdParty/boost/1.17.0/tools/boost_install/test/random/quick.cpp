
// Copyright 2017, 2018 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/random/random_device.hpp>
#include <boost/core/lightweight_test.hpp>

int main()
{
    boost::random::random_device dev;

    BOOST_TEST_NE( dev(), dev() );

    return boost::report_errors();
}
