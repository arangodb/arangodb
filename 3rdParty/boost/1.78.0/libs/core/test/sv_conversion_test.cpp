// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>
#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
# include <string_view>
#endif
#if !defined(BOOST_NO_CXX17_HDR_MEMORY_RESOURCE)
# include <memory_resource>
#endif

boost::core::string_view f( boost::core::string_view const& str )
{
    return str;
}

int main()
{
    {
        std::string s1( "123" );
        std::string s2 = f( s1 );

        BOOST_TEST_EQ( s1, s2 );
    }

#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)

    {
        std::string_view s1( "123" );
        std::string_view s2 = f( s1 );

        BOOST_TEST_EQ( s1, s2 );
    }

#endif

#if !defined(BOOST_NO_CXX17_HDR_MEMORY_RESOURCE)

    {
        using pmr_string = std::basic_string<char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>>;

        pmr_string s1( "123" );
        pmr_string s2 = f( s1 );

        BOOST_TEST_EQ( s1, s2 );
    }

#endif

    return boost::report_errors();
}
