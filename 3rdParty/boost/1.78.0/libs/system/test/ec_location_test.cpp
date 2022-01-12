// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cerrno>

int main()
{
    int const val = ENOENT;
    boost::system::error_category const & cat = boost::system::generic_category();

    {
        boost::system::error_code ec( val, cat );

        BOOST_TEST_EQ( ec.value(), val );
        BOOST_TEST_EQ( &ec.category(), &cat );

        BOOST_TEST( ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );
    }

    {
        BOOST_STATIC_CONSTEXPR boost::source_location loc = BOOST_CURRENT_LOCATION;

        boost::system::error_code ec( val, cat, &loc );

        BOOST_TEST_EQ( ec.value(), val );
        BOOST_TEST_EQ( &ec.category(), &cat );

        BOOST_TEST( ec.failed() );

        BOOST_TEST( ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 27 );
    }

    {
        boost::system::error_code ec;

        BOOST_TEST_EQ( ec.value(), 0 );
        BOOST_TEST_EQ( &ec.category(), &boost::system::system_category() );

        BOOST_TEST( !ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );

        BOOST_STATIC_CONSTEXPR boost::source_location loc = BOOST_CURRENT_LOCATION;

        ec = boost::system::error_code( val, cat, &loc );

        BOOST_TEST_EQ( ec.value(), val );
        BOOST_TEST_EQ( &ec.category(), &cat );

        BOOST_TEST( ec.failed() );

        BOOST_TEST( ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 51 );
    }

    {
        boost::system::error_code ec;

        BOOST_TEST_EQ( ec.value(), 0 );
        BOOST_TEST_EQ( &ec.category(), &boost::system::system_category() );

        BOOST_TEST( !ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );

        BOOST_STATIC_CONSTEXPR boost::source_location loc = BOOST_CURRENT_LOCATION;

        ec.assign( val, cat, &loc );

        BOOST_TEST_EQ( ec.value(), val );
        BOOST_TEST_EQ( &ec.category(), &cat );

        BOOST_TEST( ec.failed() );

        BOOST_TEST( ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 75 );
    }

#if defined(BOOST_SYSTEM_HAS_SYSTEM_ERROR)

    {
        std::error_code e2( val, std::generic_category() );

        boost::system::error_code ec( e2 );

        BOOST_TEST( ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );

        BOOST_STATIC_CONSTEXPR boost::source_location loc = BOOST_CURRENT_LOCATION;

        ec.assign( val, cat, &loc );

        BOOST_TEST_EQ( ec.value(), val );
        BOOST_TEST_EQ( &ec.category(), &cat );

        BOOST_TEST( ec.failed() );

        BOOST_TEST( ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 100 );
    }

#endif

    return boost::report_errors();
}
