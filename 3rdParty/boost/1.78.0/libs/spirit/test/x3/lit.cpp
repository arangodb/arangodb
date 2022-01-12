/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/spirit/home/x3.hpp>

#include <string>

#include "test.hpp"

int
main()
{
    using spirit_test::test_attr;
    using boost::spirit::x3::lit;
    using boost::spirit::x3::char_;

    BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(lit("x"));

    {
        std::string attr;
        auto p = char_ >> lit("\n");
        BOOST_TEST(test_attr("A\n", p, attr));
        BOOST_TEST(attr == "A");
    }

    {
        using namespace boost::spirit::x3::ascii;
        std::string attr;
        auto p = char_ >> lit("\n");
        BOOST_TEST(test_attr("A\n", p, attr));
        BOOST_TEST(attr == "A");
    }

    {
        using namespace boost::spirit::x3::iso8859_1;
        std::string attr;
        auto p = char_ >> lit("\n");
        BOOST_TEST(test_attr("É\n", p, attr));
        BOOST_TEST(attr == "É");
    }

    {
        using namespace boost::spirit::x3::standard_wide;
        std::wstring attr;
        auto p = char_ >> lit("\n");
        BOOST_TEST(test_attr(l"É\n", p, attr));
        BOOST_TEST(attr == "A");
    }

    return boost::report_errors();
}
