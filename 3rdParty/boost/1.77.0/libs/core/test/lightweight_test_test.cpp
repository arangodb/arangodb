//
// Test for lightweight_test.hpp
//
// Copyright (c) 2014 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#if defined(_MSC_VER)
# pragma warning( disable: 4702 ) // unreachable code
# pragma warning( disable: 4530 ) // unwind without /EHsc from <ostream>
# pragma warning( disable: 4577 ) // noexcept without /EHsc from <exception>
#endif

#include <boost/core/lightweight_test.hpp>
#include <vector>

struct X
{
};

#if !defined( BOOST_NO_EXCEPTIONS )
# define LWT_THROW( x ) throw x
#else
# define LWT_THROW( x ) ((void)(x))
#endif

void f( bool x )
{
    if( x )
    {
        LWT_THROW( X() );
    }
    else
    {
        LWT_THROW( 5 );
    }
}

#if defined(__GNUC__)
# pragma GCC diagnostic ignored "-Waddress"
#endif

#if defined(__clang__) && defined(__has_warning)
# if __has_warning( "-Wstring-plus-int" )
#  pragma clang diagnostic ignored "-Wstring-plus-int"
# endif
#endif

int main()
{
    int x = 0;

    // BOOST_TEST

    BOOST_TEST( x == 0 );
    BOOST_TEST( ++x == 1 );
    BOOST_TEST( x++ == 1 );
    BOOST_TEST( x == 2? true: false );
    BOOST_TEST( x == 2? &x: 0 );
    
    // BOOST_TEST_NOT
    
    BOOST_TEST_NOT( x == 1 );
    BOOST_TEST_NOT( ++x == 2 );
    BOOST_TEST_NOT( x++ == 2 );
    BOOST_TEST_NOT( --x == 2 );
    BOOST_TEST_NOT( x-- == 2 );
    BOOST_TEST_NOT( x == 2? false: true );
    BOOST_TEST_NOT( x == 2? 0: &x );

    // BOOST_TEST_EQ

    BOOST_TEST_EQ( x, 2 );
    BOOST_TEST_EQ( ++x, 3 );
    BOOST_TEST_EQ( x++, 3 );

    int y = 4;

    BOOST_TEST_EQ( ++x, ++y );
    BOOST_TEST_EQ( x++, y++ );
    BOOST_TEST_CSTR_EQ("xabc"+1, "yabc"+1); // equal cstrings, different addresses
    BOOST_TEST_EQ( &y, &y );

    // BOOST_TEST_NE

    BOOST_TEST_NE( ++x, y );
    BOOST_TEST_NE( &x, &y );
    BOOST_TEST_NE("xabc"+1, "yabc"+1); // equal cstrings, different addresses
    BOOST_TEST_CSTR_NE("x", "y");

    // BOOST_TEST_ALL_EQ
    {
        std::vector<int> xarray;
        xarray.push_back(1);
        xarray.push_back(2);
        std::vector<int> yarray(xarray);
        BOOST_TEST_ALL_EQ(xarray.begin(), xarray.end(), yarray.begin(), yarray.end());
    }

    // BOOST_TEST_THROWS

    BOOST_TEST_THROWS( throw X(), X );
    BOOST_TEST_THROWS( throw 1, int );

    BOOST_TEST_THROWS( f(true), X );
    BOOST_TEST_THROWS( f(false), int );

    // BOOST_TEST_NO_THROW

    BOOST_TEST_NO_THROW(++y);

    return boost::report_errors();
}
