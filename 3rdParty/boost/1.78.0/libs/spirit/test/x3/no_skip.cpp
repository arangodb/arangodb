/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman
    Copyright (c) 2013 Agustin Berge

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/spirit/home/x3.hpp>

#include <iostream>
#include "test.hpp"

int
main()
{
    using spirit_test::test;
    using spirit_test::test_attr;
    using boost::spirit::x3::ascii::space;
    using boost::spirit::x3::ascii::space_type;
    using boost::spirit::x3::ascii::char_;
    using boost::spirit::x3::lexeme;
    using boost::spirit::x3::no_skip;

    BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(no_skip['x']);

    // without skipping no_skip is equivalent to lexeme
    {
        std::string str;
        BOOST_TEST((test_attr("'  abc '", '\'' >> no_skip[+~char_('\'')] >> '\'', str)));
        BOOST_TEST(str == "  abc ");
    }
    {
        std::string str;
        BOOST_TEST((test_attr("'  abc '", '\'' >> lexeme[+~char_('\'')] >> '\'', str)));
        BOOST_TEST(str == "  abc ");
    }

    // with skipping, no_skip allows to match a leading skipper
    {
        std::string str;
        BOOST_TEST((test_attr("'  abc '", '\'' >> no_skip[+~char_('\'')] >> '\'', str, space)));
        BOOST_TEST(str == "  abc ");
    }
    {
        std::string str;
        BOOST_TEST((test_attr("'  abc '", '\'' >> lexeme[+~char_('\'')] >> '\'', str, space)));
        BOOST_TEST(str == "abc ");
    }

    return boost::report_errors();
}
