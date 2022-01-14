// Test for boost/core/bit.hpp (bit_cast)
//
// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/bit.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/cstdint.hpp>
#include <cstring>

int main()
{
    {
        float x = 0.89f;
        boost::uint32_t y = boost::core::bit_cast<boost::uint32_t>( x );

        BOOST_TEST( std::memcmp( &x, &y, sizeof(x) ) == 0 );
    }

    {
        double x = 0.89;
        boost::uint64_t y = boost::core::bit_cast<boost::uint64_t>( x );

        BOOST_TEST( std::memcmp( &x, &y, sizeof(x) ) == 0 );
    }

    return boost::report_errors();
}
