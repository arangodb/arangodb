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

#define TEST_LT(x, y) \
    BOOST_TEST_LT(x, y); \
    BOOST_TEST_LE(x, y); \
    BOOST_TEST_NE(x, y); \
    BOOST_TEST_NOT((x) == (y)); \
    BOOST_TEST_NOT((x) >= (y)); \
    BOOST_TEST_NOT((x) > (y)); \
    BOOST_TEST_GT(y, x); \
    BOOST_TEST_GE(y, x); \
    BOOST_TEST_NOT((y) < (x)); \
    BOOST_TEST_NOT((y) <= (x));

int main()
{
    {
        boost::core::string_view sv0( "" );
        boost::core::string_view sv1( "12" );
        boost::core::string_view sv2( "122" );
        boost::core::string_view sv3( "123" );
        boost::core::string_view sv4( "124" );
        boost::core::string_view sv5( "1234" );

        TEST_LT( sv0, sv1 );
        TEST_LT( sv1, sv2 );
        TEST_LT( sv2, sv3 );
        TEST_LT( sv3, sv4 );
        TEST_LT( sv3, sv5 );
        TEST_LT( sv5, sv4 );

        TEST_LT( sv0, std::string( "12" ) );
        TEST_LT( sv1, std::string( "122" ) );
        TEST_LT( sv2, std::string( "123" ) );
        TEST_LT( sv3, std::string( "124" ) );
        TEST_LT( sv3, std::string( "1234" ) );
        TEST_LT( sv5, std::string( "124" ) );

        TEST_LT( sv0, "12" );
        TEST_LT( sv1, "122" );
        TEST_LT( sv2, "123" );
        TEST_LT( sv3, "124" );
        TEST_LT( sv3, "1234" );
        TEST_LT( sv5, "124" );

#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)

        TEST_LT( sv0, std::string_view( "12" ) );
        TEST_LT( sv1, std::string_view( "122" ) );
        TEST_LT( sv2, std::string_view( "123" ) );
        TEST_LT( sv3, std::string_view( "124" ) );
        TEST_LT( sv3, std::string_view( "1234" ) );
        TEST_LT( sv5, std::string_view( "124" ) );

#endif

        TEST_LT( std::string( "" ), sv1 );
        TEST_LT( std::string( "12" ), sv2 );
        TEST_LT( std::string( "122" ), sv3 );
        TEST_LT( std::string( "123" ), sv4 );
        TEST_LT( std::string( "123" ), sv5 );
        TEST_LT( std::string( "1234" ), sv4 );

        TEST_LT( "", sv1 );
        TEST_LT( "12", sv2 );
        TEST_LT( "122", sv3 );
        TEST_LT( "123", sv4 );
        TEST_LT( "123", sv5 );
        TEST_LT( "1234", sv4 );

#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)

        TEST_LT( std::string_view( "" ), sv1 );
        TEST_LT( std::string_view( "12" ), sv2 );
        TEST_LT( std::string_view( "122" ), sv3 );
        TEST_LT( std::string_view( "123" ), sv4 );
        TEST_LT( std::string_view( "123" ), sv5 );
        TEST_LT( std::string_view( "1234" ), sv4 );

#endif

#if !defined(BOOST_NO_CXX17_HDR_MEMORY_RESOURCE)

        using pmr_string = std::basic_string<char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>>;

        TEST_LT( pmr_string( "" ), sv1 );
        TEST_LT( pmr_string( "12" ), sv2 );
        TEST_LT( pmr_string( "122" ), sv3 );
        TEST_LT( pmr_string( "123" ), sv4 );
        TEST_LT( pmr_string( "123" ), sv5 );
        TEST_LT( pmr_string( "1234" ), sv4 );

#endif
    }

    return boost::report_errors();
}
