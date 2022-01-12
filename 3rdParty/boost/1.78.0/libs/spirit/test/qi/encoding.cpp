/*=============================================================================
    Copyright (c) 2001-2011 Joel de Guzman
    http://spirit.sourceforge.net/

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/spirit/include/qi_char.hpp>
#include <boost/spirit/include/qi_string.hpp>
#include <boost/spirit/include/qi_directive.hpp>
#include <boost/spirit/include/qi_action.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>

#include <iostream>
#include "test.hpp"

int
main()
{
    using spirit_test::test;
    using spirit_test::test_attr;
    using boost::spirit::qi::lit;
    using boost::spirit::qi::no_case;
    using boost::spirit::qi::char_;
    using boost::spirit::qi::encoding;
    namespace char_encoding = boost::spirit::char_encoding;

    encoding<char_encoding::iso8859_1> iso8859_1;

    { // test extended ASCII characters
        BOOST_TEST(test("\xC1", iso8859_1[no_case['\xE1']]));
        BOOST_TEST(test("\xC1", iso8859_1[no_case[char_('\xE1')]]));

        BOOST_TEST(test("\xC9", iso8859_1[no_case[char_("\xE5-\xEF")]]));
        BOOST_TEST(!test("\xFF", iso8859_1[no_case[char_("\xE5-\xEF")]]));

        BOOST_TEST(test("\xC1\xE1", iso8859_1[no_case["\xE1\xC1"]]));
        BOOST_TEST(test("\xC1\xE1", iso8859_1[no_case[lit("\xE1\xC1")]]));
    }

    return boost::report_errors();
}
