
// Copyright 2018, 2020 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

// See library home page at http://www.boost.org/libs/system

// Avoid spurious VC++ warnings
#define _CRT_SECURE_NO_WARNINGS

#include <boost/system/error_category.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstdio>

//

namespace sys = boost::system;

class user_category: public sys::error_category
{
public:

    virtual const char * name() const BOOST_NOEXCEPT
    {
        return "user";
    }

    virtual std::string message( int ev ) const
    {
        char buffer[ 256 ];
        std::sprintf( buffer, "user message %d", ev );

        return buffer;
    }

    using sys::error_category::message;
};

static user_category s_cat_1;
static user_category s_cat_2;

int main()
{
    // default_error_condition

    BOOST_TEST( s_cat_1.default_error_condition( 1 ).value() == 1 );
    BOOST_TEST( s_cat_1.default_error_condition( 1 ).category() == s_cat_1 );

    BOOST_TEST( s_cat_2.default_error_condition( 2 ).value() == 2 );
    BOOST_TEST( s_cat_2.default_error_condition( 2 ).category() == s_cat_2 );

    // equivalent

    BOOST_TEST( s_cat_1.equivalent( 1, s_cat_1.default_error_condition( 1 ) ) );
    BOOST_TEST( !s_cat_1.equivalent( 1, s_cat_1.default_error_condition( 2 ) ) );
    BOOST_TEST( !s_cat_1.equivalent( 1, s_cat_2.default_error_condition( 2 ) ) );

    // message

    {
        char buffer[ 256 ];
        BOOST_TEST_CSTR_EQ( s_cat_1.message( 1, buffer, sizeof( buffer ) ), s_cat_1.message( 1 ).c_str() );
    }

    {
        char buffer[ 4 ];
        BOOST_TEST_CSTR_EQ( s_cat_1.message( 1, buffer, sizeof( buffer ) ), "use" );
    }

    // ==

    BOOST_TEST_NOT( s_cat_1 == s_cat_2 );
    BOOST_TEST( s_cat_1 != s_cat_2 );

    return boost::report_errors();
}
