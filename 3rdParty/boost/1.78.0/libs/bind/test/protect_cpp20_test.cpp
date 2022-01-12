// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt)

#include <boost/bind/protect.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <functional>

//

int main()
{
    BOOST_TEST_EQ( boost::protect( std::plus<int>() )( 1, 2 ), 3 );
    BOOST_TEST_EQ( boost::protect( std::minus<int>() )( 1, 2 ), -1 );
    BOOST_TEST_EQ( boost::protect( std::multiplies<int>() )( 1, 2 ), 2 );
    BOOST_TEST_EQ( boost::protect( std::divides<int>() )( 1, 2 ), 0 );
    BOOST_TEST_EQ( boost::protect( std::modulus<int>() )( 1, 2 ), 1 );
    BOOST_TEST_EQ( boost::protect( std::negate<int>() )( 1 ), -1 );

    BOOST_TEST_EQ( boost::protect( std::equal_to<int>() )( 1, 2 ), false );
    BOOST_TEST_EQ( boost::protect( std::not_equal_to<int>() )( 1, 2 ), true );
    BOOST_TEST_EQ( boost::protect( std::greater<int>() )( 1, 2 ), false );
    BOOST_TEST_EQ( boost::protect( std::less<int>() )( 1, 2 ), true );
    BOOST_TEST_EQ( boost::protect( std::greater_equal<int>() )( 1, 2 ), false );
    BOOST_TEST_EQ( boost::protect( std::less_equal<int>() )( 1, 2 ), true );

    BOOST_TEST_EQ( boost::protect( std::logical_and<int>() )( 1, 2 ), true );
    BOOST_TEST_EQ( boost::protect( std::logical_or<int>() )( 1, 2 ), true );
    BOOST_TEST_EQ( boost::protect( std::logical_not<int>() )( 1 ), false );

#if !BOOST_WORKAROUND(BOOST_MSVC, < 1600)

    BOOST_TEST_EQ( boost::protect( std::bit_and<int>() )( 1, 2 ), 0 );
    BOOST_TEST_EQ( boost::protect( std::bit_or<int>() )( 1, 2 ), 3 );
    BOOST_TEST_EQ( boost::protect( std::bit_xor<int>() )( 1, 2 ), 3 );

#endif

    return boost::report_errors();
}
