// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iterator>
#include <string>
#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
# include <string_view>
#endif
#if !defined(BOOST_NO_CXX17_HDR_MEMORY_RESOURCE)
# include <memory_resource>
#endif

template<class It> std::reverse_iterator<It> make_reverse_iterator( It it )
{
    return std::reverse_iterator<It>( it );
}

int main()
{
    {
        boost::core::string_view sv;

        BOOST_TEST_EQ( sv.data(), static_cast<char const*>(0) );
        BOOST_TEST_EQ( sv.size(), 0 );

        BOOST_TEST_EQ( sv.begin(), sv.data() );
        BOOST_TEST_EQ( sv.end(), sv.data() + sv.size() );

        BOOST_TEST_EQ( sv.cbegin(), sv.data() );
        BOOST_TEST_EQ( sv.cend(), sv.data() + sv.size() );

        BOOST_TEST( sv.rbegin() == ::make_reverse_iterator( sv.end() ) );
        BOOST_TEST( sv.rend() == ::make_reverse_iterator( sv.begin() ) );

        BOOST_TEST( sv.crbegin() == ::make_reverse_iterator( sv.cend() ) );
        BOOST_TEST( sv.crend() == ::make_reverse_iterator( sv.cbegin() ) );

        BOOST_TEST_EQ( sv.length(), sv.size() );
        BOOST_TEST_EQ( sv.empty(), sv.size() == 0 );

        BOOST_TEST_EQ( sv.max_size(), boost::core::string_view::npos );
    }

    {
        char const* s = "";

        boost::core::string_view sv( s );

        BOOST_TEST_EQ( sv.data(), s );
        BOOST_TEST_EQ( sv.size(), 0 );

        BOOST_TEST_EQ( sv.begin(), sv.data() );
        BOOST_TEST_EQ( sv.end(), sv.data() + sv.size() );

        BOOST_TEST_EQ( sv.cbegin(), sv.data() );
        BOOST_TEST_EQ( sv.cend(), sv.data() + sv.size() );

        BOOST_TEST( sv.rbegin() == ::make_reverse_iterator( sv.end() ) );
        BOOST_TEST( sv.rend() == ::make_reverse_iterator( sv.begin() ) );

        BOOST_TEST( sv.crbegin() == ::make_reverse_iterator( sv.cend() ) );
        BOOST_TEST( sv.crend() == ::make_reverse_iterator( sv.cbegin() ) );

        BOOST_TEST_EQ( sv.length(), sv.size() );
        BOOST_TEST_EQ( sv.empty(), sv.size() == 0 );

        BOOST_TEST_EQ( sv.max_size(), boost::core::string_view::npos );

        if( !sv.empty() )
        {
            BOOST_TEST_EQ( &sv.front(), sv.data() );
            BOOST_TEST_EQ( &sv.back(), sv.data() + sv.size() - 1 );
        }
    }

    {
        char const* s = "123";

        boost::core::string_view sv( s );

        BOOST_TEST_EQ( sv.data(), s );
        BOOST_TEST_EQ( sv.size(), 3 );

        BOOST_TEST_EQ( sv.begin(), sv.data() );
        BOOST_TEST_EQ( sv.end(), sv.data() + sv.size() );

        BOOST_TEST_EQ( sv.cbegin(), sv.data() );
        BOOST_TEST_EQ( sv.cend(), sv.data() + sv.size() );

        BOOST_TEST( sv.rbegin() == ::make_reverse_iterator( sv.end() ) );
        BOOST_TEST( sv.rend() == ::make_reverse_iterator( sv.begin() ) );

        BOOST_TEST( sv.crbegin() == ::make_reverse_iterator( sv.cend() ) );
        BOOST_TEST( sv.crend() == ::make_reverse_iterator( sv.cbegin() ) );

        BOOST_TEST_EQ( sv.length(), sv.size() );
        BOOST_TEST_EQ( sv.empty(), sv.size() == 0 );

        BOOST_TEST_EQ( sv.max_size(), boost::core::string_view::npos );

        if( !sv.empty() )
        {
            BOOST_TEST_EQ( &sv.front(), sv.data() );
            BOOST_TEST_EQ( &sv.back(), sv.data() + sv.size() - 1 );
        }
    }

    {
        char const* s = "123";

        boost::core::string_view sv( s, 0 );

        BOOST_TEST_EQ( sv.data(), s );
        BOOST_TEST_EQ( sv.size(), 0 );

        BOOST_TEST_EQ( sv.begin(), sv.data() );
        BOOST_TEST_EQ( sv.end(), sv.data() + sv.size() );

        BOOST_TEST_EQ( sv.cbegin(), sv.data() );
        BOOST_TEST_EQ( sv.cend(), sv.data() + sv.size() );

        BOOST_TEST( sv.rbegin() == ::make_reverse_iterator( sv.end() ) );
        BOOST_TEST( sv.rend() == ::make_reverse_iterator( sv.begin() ) );

        BOOST_TEST( sv.crbegin() == ::make_reverse_iterator( sv.cend() ) );
        BOOST_TEST( sv.crend() == ::make_reverse_iterator( sv.cbegin() ) );

        BOOST_TEST_EQ( sv.length(), sv.size() );
        BOOST_TEST_EQ( sv.empty(), sv.size() == 0 );

        BOOST_TEST_EQ( sv.max_size(), boost::core::string_view::npos );

        if( !sv.empty() )
        {
            BOOST_TEST_EQ( &sv.front(), sv.data() );
            BOOST_TEST_EQ( &sv.back(), sv.data() + sv.size() - 1 );
        }
    }

    {
        char const* s = "123";

        boost::core::string_view sv( s, 2 );

        BOOST_TEST_EQ( sv.data(), s );
        BOOST_TEST_EQ( sv.size(), 2 );

        BOOST_TEST_EQ( sv.begin(), sv.data() );
        BOOST_TEST_EQ( sv.end(), sv.data() + sv.size() );

        BOOST_TEST_EQ( sv.cbegin(), sv.data() );
        BOOST_TEST_EQ( sv.cend(), sv.data() + sv.size() );

        BOOST_TEST( sv.rbegin() == ::make_reverse_iterator( sv.end() ) );
        BOOST_TEST( sv.rend() == ::make_reverse_iterator( sv.begin() ) );

        BOOST_TEST( sv.crbegin() == ::make_reverse_iterator( sv.cend() ) );
        BOOST_TEST( sv.crend() == ::make_reverse_iterator( sv.cbegin() ) );

        BOOST_TEST_EQ( sv.length(), sv.size() );
        BOOST_TEST_EQ( sv.empty(), sv.size() == 0 );

        BOOST_TEST_EQ( sv.max_size(), boost::core::string_view::npos );

        if( !sv.empty() )
        {
            BOOST_TEST_EQ( &sv.front(), sv.data() );
            BOOST_TEST_EQ( &sv.back(), sv.data() + sv.size() - 1 );
        }
    }

    {
        char const* s = "123";

        boost::core::string_view sv( s, s );

        BOOST_TEST_EQ( sv.data(), s );
        BOOST_TEST_EQ( sv.size(), 0 );

        BOOST_TEST_EQ( sv.begin(), sv.data() );
        BOOST_TEST_EQ( sv.end(), sv.data() + sv.size() );

        BOOST_TEST_EQ( sv.cbegin(), sv.data() );
        BOOST_TEST_EQ( sv.cend(), sv.data() + sv.size() );

        BOOST_TEST( sv.rbegin() == ::make_reverse_iterator( sv.end() ) );
        BOOST_TEST( sv.rend() == ::make_reverse_iterator( sv.begin() ) );

        BOOST_TEST( sv.crbegin() == ::make_reverse_iterator( sv.cend() ) );
        BOOST_TEST( sv.crend() == ::make_reverse_iterator( sv.cbegin() ) );

        BOOST_TEST_EQ( sv.length(), sv.size() );
        BOOST_TEST_EQ( sv.empty(), sv.size() == 0 );

        BOOST_TEST_EQ( sv.max_size(), boost::core::string_view::npos );

        if( !sv.empty() )
        {
            BOOST_TEST_EQ( &sv.front(), sv.data() );
            BOOST_TEST_EQ( &sv.back(), sv.data() + sv.size() - 1 );
        }
    }

    {
        char const* s = "123";

        boost::core::string_view sv( s, s + 2 );

        BOOST_TEST_EQ( sv.data(), s );
        BOOST_TEST_EQ( sv.size(), 2 );

        BOOST_TEST_EQ( sv.begin(), sv.data() );
        BOOST_TEST_EQ( sv.end(), sv.data() + sv.size() );

        BOOST_TEST_EQ( sv.cbegin(), sv.data() );
        BOOST_TEST_EQ( sv.cend(), sv.data() + sv.size() );

        BOOST_TEST( sv.rbegin() == ::make_reverse_iterator( sv.end() ) );
        BOOST_TEST( sv.rend() == ::make_reverse_iterator( sv.begin() ) );

        BOOST_TEST( sv.crbegin() == ::make_reverse_iterator( sv.cend() ) );
        BOOST_TEST( sv.crend() == ::make_reverse_iterator( sv.cbegin() ) );

        BOOST_TEST_EQ( sv.length(), sv.size() );
        BOOST_TEST_EQ( sv.empty(), sv.size() == 0 );

        BOOST_TEST_EQ( sv.max_size(), boost::core::string_view::npos );

        if( !sv.empty() )
        {
            BOOST_TEST_EQ( &sv.front(), sv.data() );
            BOOST_TEST_EQ( &sv.back(), sv.data() + sv.size() - 1 );
        }
    }

    {
        std::string str = "123";

        boost::core::string_view sv( str );

        BOOST_TEST_EQ( sv.data(), str.data() );
        BOOST_TEST_EQ( sv.size(), str.size() );

        BOOST_TEST_EQ( sv.begin(), sv.data() );
        BOOST_TEST_EQ( sv.end(), sv.data() + sv.size() );

        BOOST_TEST_EQ( sv.cbegin(), sv.data() );
        BOOST_TEST_EQ( sv.cend(), sv.data() + sv.size() );

        BOOST_TEST( sv.rbegin() == ::make_reverse_iterator( sv.end() ) );
        BOOST_TEST( sv.rend() == ::make_reverse_iterator( sv.begin() ) );

        BOOST_TEST( sv.crbegin() == ::make_reverse_iterator( sv.cend() ) );
        BOOST_TEST( sv.crend() == ::make_reverse_iterator( sv.cbegin() ) );

        BOOST_TEST_EQ( sv.length(), sv.size() );
        BOOST_TEST_EQ( sv.empty(), sv.size() == 0 );

        BOOST_TEST_EQ( sv.max_size(), boost::core::string_view::npos );

        if( !sv.empty() )
        {
            BOOST_TEST_EQ( &sv.front(), sv.data() );
            BOOST_TEST_EQ( &sv.back(), sv.data() + sv.size() - 1 );
        }
    }

#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)

    {
        std::string_view str = "123";

        boost::core::string_view sv( str );

        BOOST_TEST_EQ( sv.data(), str.data() );
        BOOST_TEST_EQ( sv.size(), str.size() );

        BOOST_TEST_EQ( sv.begin(), sv.data() );
        BOOST_TEST_EQ( sv.end(), sv.data() + sv.size() );

        BOOST_TEST_EQ( sv.cbegin(), sv.data() );
        BOOST_TEST_EQ( sv.cend(), sv.data() + sv.size() );

        BOOST_TEST( sv.rbegin() == ::make_reverse_iterator( sv.end() ) );
        BOOST_TEST( sv.rend() == ::make_reverse_iterator( sv.begin() ) );

        BOOST_TEST( sv.crbegin() == ::make_reverse_iterator( sv.cend() ) );
        BOOST_TEST( sv.crend() == ::make_reverse_iterator( sv.cbegin() ) );

        BOOST_TEST_EQ( sv.length(), sv.size() );
        BOOST_TEST_EQ( sv.empty(), sv.size() == 0 );

        BOOST_TEST_EQ( sv.max_size(), boost::core::string_view::npos );

        if( !sv.empty() )
        {
            BOOST_TEST_EQ( &sv.front(), sv.data() );
            BOOST_TEST_EQ( &sv.back(), sv.data() + sv.size() - 1 );
        }
    }

#endif

#if !defined(BOOST_NO_CXX17_HDR_MEMORY_RESOURCE)

    {
        using pmr_string = std::basic_string<char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>>;

        pmr_string str = "123";

        boost::core::string_view sv( str );

        BOOST_TEST_EQ( sv.data(), str.data() );
        BOOST_TEST_EQ( sv.size(), str.size() );

        BOOST_TEST_EQ( sv.begin(), sv.data() );
        BOOST_TEST_EQ( sv.end(), sv.data() + sv.size() );

        BOOST_TEST_EQ( sv.cbegin(), sv.data() );
        BOOST_TEST_EQ( sv.cend(), sv.data() + sv.size() );

        BOOST_TEST( sv.rbegin() == ::make_reverse_iterator( sv.end() ) );
        BOOST_TEST( sv.rend() == ::make_reverse_iterator( sv.begin() ) );

        BOOST_TEST( sv.crbegin() == ::make_reverse_iterator( sv.cend() ) );
        BOOST_TEST( sv.crend() == ::make_reverse_iterator( sv.cbegin() ) );

        BOOST_TEST_EQ( sv.length(), sv.size() );
        BOOST_TEST_EQ( sv.empty(), sv.size() == 0 );

        BOOST_TEST_EQ( sv.max_size(), boost::core::string_view::npos );

        if( !sv.empty() )
        {
            BOOST_TEST_EQ( &sv.front(), sv.data() );
            BOOST_TEST_EQ( &sv.back(), sv.data() + sv.size() - 1 );
        }
    }

#endif

    return boost::report_errors();
}
