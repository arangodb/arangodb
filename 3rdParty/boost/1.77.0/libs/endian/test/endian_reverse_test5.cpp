// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/endian/conversion.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <cstddef>

int main()
{
    int v[] = { 1, 2 };

    boost::endian::endian_reverse_inplace( v );
    boost::endian::endian_reverse_inplace( v );
    BOOST_TEST_EQ( v[0], 1 );
    BOOST_TEST_EQ( v[1], 2 );

    boost::endian::native_to_little_inplace( v );
    boost::endian::little_to_native_inplace( v );
    BOOST_TEST_EQ( v[0], 1 );
    BOOST_TEST_EQ( v[1], 2 );

    boost::endian::native_to_big_inplace( v );
    boost::endian::big_to_native_inplace( v );
    BOOST_TEST_EQ( v[0], 1 );
    BOOST_TEST_EQ( v[1], 2 );

    return boost::report_errors();
}
