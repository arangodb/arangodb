// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <stdexcept>
#include <cstddef>

int main()
{
    {
        boost::core::string_view sv;

        {
            boost::core::string_view sv2 = sv.substr();

            BOOST_TEST_EQ( sv2.data(), sv.data() );
            BOOST_TEST_EQ( sv2.size(), sv.size() );
        }

        {
            boost::core::string_view sv2 = sv.substr( 0 );

            BOOST_TEST_EQ( sv2.data(), sv.data() );
            BOOST_TEST_EQ( sv2.size(), sv.size() );
        }

        BOOST_TEST_THROWS( sv.substr( 1 ), std::out_of_range );
        BOOST_TEST_THROWS( sv.substr( boost::core::string_view::npos ), std::out_of_range );

        {
            boost::core::string_view sv2 = sv.substr( 0, 1 );

            BOOST_TEST_EQ( sv2.data(), sv.data() );
            BOOST_TEST_EQ( sv2.size(), sv.size() );
        }

        {
            boost::core::string_view sv2 = sv.substr( 0, 0 );

            BOOST_TEST_EQ( sv2.data(), sv.data() );
            BOOST_TEST_EQ( sv2.size(), sv.size() );
        }

        BOOST_TEST_THROWS( sv.substr( 1, 0 ), std::out_of_range );
        BOOST_TEST_THROWS( sv.substr( boost::core::string_view::npos, 0 ), std::out_of_range );
    }

    {
        boost::core::string_view sv( "12345" );

        {
            boost::core::string_view sv2 = sv.substr();

            BOOST_TEST_EQ( sv2.data(), sv.data() );
            BOOST_TEST_EQ( sv2.size(), sv.size() );
        }

        {
            boost::core::string_view sv2 = sv.substr( 0 );

            BOOST_TEST_EQ( sv2.data(), sv.data() );
            BOOST_TEST_EQ( sv2.size(), sv.size() );
        }

        {
            boost::core::string_view sv2 = sv.substr( 2 );

            BOOST_TEST_EQ( sv2.data(), sv.data() + 2 );
            BOOST_TEST_EQ( sv2.size(), sv.size() - 2 );
        }

        {
            boost::core::string_view sv2 = sv.substr( 5 );

            BOOST_TEST_EQ( sv2.data(), sv.data() + 5 );
            BOOST_TEST_EQ( sv2.size(), sv.size() - 5 );
        }

        BOOST_TEST_THROWS( sv.substr( 6 ), std::out_of_range );
        BOOST_TEST_THROWS( sv.substr( boost::core::string_view::npos ), std::out_of_range );

        {
            boost::core::string_view sv2 = sv.substr( 0, 0 );

            BOOST_TEST_EQ( sv2.data(), sv.data() );
            BOOST_TEST_EQ( sv2.size(), 0 );
        }

        {
            boost::core::string_view sv2 = sv.substr( 2, 0 );

            BOOST_TEST_EQ( sv2.data(), sv.data() + 2 );
            BOOST_TEST_EQ( sv2.size(), 0 );
        }

        {
            boost::core::string_view sv2 = sv.substr( 5, 0 );

            BOOST_TEST_EQ( sv2.data(), sv.data() + 5 );
            BOOST_TEST_EQ( sv2.size(), 0 );
        }

        BOOST_TEST_THROWS( sv.substr( 6, 0 ), std::out_of_range );
        BOOST_TEST_THROWS( sv.substr( boost::core::string_view::npos, 0 ), std::out_of_range );

        {
            boost::core::string_view sv2 = sv.substr( 0, 3 );

            BOOST_TEST_EQ( sv2.data(), sv.data() );
            BOOST_TEST_EQ( sv2.size(), 3 );
        }

        {
            boost::core::string_view sv2 = sv.substr( 2, 3 );

            BOOST_TEST_EQ( sv2.data(), sv.data() + 2 );
            BOOST_TEST_EQ( sv2.size(), 3 );
        }

        {
            boost::core::string_view sv2 = sv.substr( 4, 3 );

            BOOST_TEST_EQ( sv2.data(), sv.data() + 4 );
            BOOST_TEST_EQ( sv2.size(), 1 );
        }

        {
            boost::core::string_view sv2 = sv.substr( 5, 3 );

            BOOST_TEST_EQ( sv2.data(), sv.data() + 5 );
            BOOST_TEST_EQ( sv2.size(), 0 );
        }

        BOOST_TEST_THROWS( sv.substr( 6, 3 ), std::out_of_range );
        BOOST_TEST_THROWS( sv.substr( boost::core::string_view::npos, 3 ), std::out_of_range );

        {
            boost::core::string_view sv2 = sv.substr( 0, 5 );

            BOOST_TEST_EQ( sv2.data(), sv.data() );
            BOOST_TEST_EQ( sv2.size(), 5 );
        }

        {
            boost::core::string_view sv2 = sv.substr( 2, 5 );

            BOOST_TEST_EQ( sv2.data(), sv.data() + 2 );
            BOOST_TEST_EQ( sv2.size(), 3 );
        }

        {
            boost::core::string_view sv2 = sv.substr( 4, 5 );

            BOOST_TEST_EQ( sv2.data(), sv.data() + 4 );
            BOOST_TEST_EQ( sv2.size(), 1 );
        }

        {
            boost::core::string_view sv2 = sv.substr( 5, 5 );

            BOOST_TEST_EQ( sv2.data(), sv.data() + 5 );
            BOOST_TEST_EQ( sv2.size(), 0 );
        }

        BOOST_TEST_THROWS( sv.substr( 6, 5 ), std::out_of_range );
        BOOST_TEST_THROWS( sv.substr( boost::core::string_view::npos, 5 ), std::out_of_range );

        {
            boost::core::string_view sv2 = sv.substr( 0, 8 );

            BOOST_TEST_EQ( sv2.data(), sv.data() );
            BOOST_TEST_EQ( sv2.size(), 5 );
        }

        {
            boost::core::string_view sv2 = sv.substr( 2, 8 );

            BOOST_TEST_EQ( sv2.data(), sv.data() + 2 );
            BOOST_TEST_EQ( sv2.size(), 3 );
        }

        {
            boost::core::string_view sv2 = sv.substr( 4, 8 );

            BOOST_TEST_EQ( sv2.data(), sv.data() + 4 );
            BOOST_TEST_EQ( sv2.size(), 1 );
        }

        {
            boost::core::string_view sv2 = sv.substr( 5, 8 );

            BOOST_TEST_EQ( sv2.data(), sv.data() + 5 );
            BOOST_TEST_EQ( sv2.size(), 0 );
        }

        BOOST_TEST_THROWS( sv.substr( 6, 8 ), std::out_of_range );
        BOOST_TEST_THROWS( sv.substr( boost::core::string_view::npos, 8 ), std::out_of_range );
    }

    return boost::report_errors();
}
