
// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/integer_sequence.hpp>
#include <boost/version.hpp>
#include <boost/core/lightweight_test.hpp>

int main()
{
    BOOST_TEST_EQ( BOOST_MP11_VERSION, BOOST_VERSION );
    return boost::report_errors();
}
