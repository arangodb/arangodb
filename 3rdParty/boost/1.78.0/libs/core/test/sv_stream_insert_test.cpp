// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <sstream>
#include <iomanip>

int main()
{
    using boost::core::string_view;

    {
        std::ostringstream os;

        os << string_view( "" );

        BOOST_TEST_EQ( os.str(), std::string( "" ) );
    }

    {
        std::ostringstream os;

        os << string_view( "123" );

        BOOST_TEST_EQ( os.str(), std::string( "123" ) );
    }

    {
        std::ostringstream os;

        os << std::setw( 5 ) << string_view( "" );

        BOOST_TEST_EQ( os.str(), std::string( "     " ) );
    }

    {
        std::ostringstream os;

        os << std::setfill( '-' ) << std::setw( 5 ) << string_view( "" );

        BOOST_TEST_EQ( os.str(), std::string( "-----" ) );
    }

    {
        std::ostringstream os;

        os << std::setw( 5 ) << string_view( "123" );

        BOOST_TEST_EQ( os.str(), std::string( "  123" ) );
    }

    {
        std::ostringstream os;

        os << std::left << std::setw( 5 ) << string_view( "123" );

        BOOST_TEST_EQ( os.str(), std::string( "123  " ) );
    }

    {
        std::ostringstream os;

        os << std::right << std::setw( 5 ) << string_view( "123" );

        BOOST_TEST_EQ( os.str(), std::string( "  123" ) );
    }

    {
        std::ostringstream os;

        os << std::setfill( '-' ) << std::setw( 5 ) << string_view( "123" );

        BOOST_TEST_EQ( os.str(), std::string( "--123" ) );
    }

    {
        std::ostringstream os;

        os << std::setfill( '-' ) << std::left << std::setw( 5 ) << string_view( "123" );

        BOOST_TEST_EQ( os.str(), std::string( "123--" ) );
    }

    {
        std::ostringstream os;

        os << std::setfill( '-' ) << std::right << std::setw( 5 ) << string_view( "123" );

        BOOST_TEST_EQ( os.str(), std::string( "--123" ) );
    }

    {
        std::ostringstream os;

        os << std::setw( 5 ) << string_view( "12345" );

        BOOST_TEST_EQ( os.str(), std::string( "12345" ) );
    }

    {
        std::ostringstream os;

        os << std::setw( 5 ) << string_view( "1234567" );

        BOOST_TEST_EQ( os.str(), std::string( "1234567" ) );
    }

    return boost::report_errors();
}
