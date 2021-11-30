// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/bind/bind.hpp>
#include <boost/core/lightweight_test.hpp>
#include <functional>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_NO_CXX11_HDR_FUNCTIONAL)

BOOST_PRAGMA_MESSAGE( "Skipping test due to BOOST_NO_CXX11_HDR_FUNCTIONAL being defined" )
int main() {}

#elif defined(BOOST_NO_CXX11_DECLTYPE)

BOOST_PRAGMA_MESSAGE( "Skipping test due to BOOST_NO_CXX11_DECLTYPE being defined" )
int main() {}

#elif defined(BOOST_NO_CXX11_HDR_TYPE_TRAITS)

BOOST_PRAGMA_MESSAGE( "Skipping test due to BOOST_NO_CXX11_HDR_TYPE_TRAITS being defined" )
int main() {}

#else

int f( int x )
{
    return -x;
}

int main()
{
    using namespace std::placeholders;

    BOOST_TEST_EQ( boost::bind( f, _1 )( 1 ), -1 );
    BOOST_TEST_EQ( boost::bind( f, _2 )( 1, 2 ), -2 );
    BOOST_TEST_EQ( boost::bind( f, _3 )( 1, 2, 3 ), -3 );
    BOOST_TEST_EQ( boost::bind( f, _4 )( 1, 2, 3, 4 ), -4 );
    BOOST_TEST_EQ( boost::bind( f, _5 )( 1, 2, 3, 4, 5 ), -5 );
    BOOST_TEST_EQ( boost::bind( f, _6 )( 1, 2, 3, 4, 5, 6 ), -6 );
    BOOST_TEST_EQ( boost::bind( f, _7 )( 1, 2, 3, 4, 5, 6, 7 ), -7 );
    BOOST_TEST_EQ( boost::bind( f, _8 )( 1, 2, 3, 4, 5, 6, 7, 8 ), -8 );
    BOOST_TEST_EQ( boost::bind( f, _9 )( 1, 2, 3, 4, 5, 6, 7, 8, 9 ), -9 );

    return boost::report_errors();
}

#endif
