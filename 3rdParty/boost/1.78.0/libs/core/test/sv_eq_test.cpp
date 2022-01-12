// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>
#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
# include <string_view>
#endif
#if !defined(BOOST_NO_CXX17_HDR_MEMORY_RESOURCE)
# include <memory_resource>
#endif

#define TEST_EQ(x, y) \
    BOOST_TEST_EQ(x, y); \
    BOOST_TEST_NOT((x) != (y)); \
    BOOST_TEST_LE(x, y); \
    BOOST_TEST_GE(x, y); \
    BOOST_TEST_NOT((x) < (y)); \
    BOOST_TEST_NOT((x) > (y))

#define TEST_NE(x, y) \
    BOOST_TEST_NE(x, y); \
    BOOST_TEST_NOT((x) == (y)); \
    BOOST_TEST((x) < (y) || (x) > (y));

int main()
{
    {
        boost::core::string_view sv1( "" );
        boost::core::string_view sv2( "" );
        boost::core::string_view sv3( "123" );
        boost::core::string_view sv4( "123" );
        boost::core::string_view sv5( "12345" );
        boost::core::string_view sv6( "12345" );

        TEST_EQ( sv1, sv1 );
        TEST_EQ( sv1, sv2 );
        TEST_NE( sv1, sv3 );
        TEST_NE( sv1, sv5 );

        TEST_EQ( sv3, sv3 );
        TEST_EQ( sv3, sv4 );
        TEST_NE( sv3, sv1 );
        TEST_NE( sv3, sv5 );

        TEST_EQ( sv5, sv5 );
        TEST_EQ( sv5, sv6 );
        TEST_NE( sv5, sv1 );
        TEST_NE( sv5, sv3 );

        BOOST_TEST_EQ( sv1, std::string( "" ) );
        BOOST_TEST_EQ( std::string( "" ), sv1 );

        BOOST_TEST_NE( sv1, std::string( "1" ) );
        BOOST_TEST_NE( std::string( "1" ), sv1 );

        BOOST_TEST_EQ( sv3, std::string( "123" ) );
        BOOST_TEST_EQ( std::string( "123" ), sv3 );

        BOOST_TEST_NE( sv3, std::string( "122" ) );
        BOOST_TEST_NE( std::string( "122" ), sv3 );

        BOOST_TEST_EQ( sv1, "" );
        BOOST_TEST_EQ( "", sv1 );

        BOOST_TEST_NE( sv1, "1" );
        BOOST_TEST_NE( "1", sv1 );

        BOOST_TEST_EQ( sv3, "123" );
        BOOST_TEST_EQ( "123", sv3 );

        BOOST_TEST_NE( sv3, "122" );
        BOOST_TEST_NE( "122", sv3 );

#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)

        BOOST_TEST_EQ( sv1, std::string_view( "" ) );
        BOOST_TEST_EQ( std::string_view( "" ), sv1 );

        BOOST_TEST_NE( sv1, std::string_view( "1" ) );
        BOOST_TEST_NE( std::string_view( "1" ), sv1 );

        BOOST_TEST_EQ( sv3, std::string_view( "123" ) );
        BOOST_TEST_EQ( std::string_view( "123" ), sv3 );

        BOOST_TEST_NE( sv3, std::string_view( "122" ) );
        BOOST_TEST_NE( std::string_view( "122" ), sv3 );

#endif

#if !defined(BOOST_NO_CXX17_HDR_MEMORY_RESOURCE)

        using pmr_string = std::basic_string<char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>>;

        BOOST_TEST_EQ( sv1, pmr_string( "" ) );
        BOOST_TEST_EQ( pmr_string( "" ), sv1 );

        BOOST_TEST_NE( sv1, pmr_string( "1" ) );
        BOOST_TEST_NE( pmr_string( "1" ), sv1 );

        BOOST_TEST_EQ( sv3, pmr_string( "123" ) );
        BOOST_TEST_EQ( pmr_string( "123" ), sv3 );

        BOOST_TEST_NE( sv3, pmr_string( "122" ) );
        BOOST_TEST_NE( pmr_string( "122" ), sv3 );

#endif
    }

    return boost::report_errors();
}
