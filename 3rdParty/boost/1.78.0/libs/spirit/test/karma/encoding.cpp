//  Copyright (c) 2001-2011 Hartmut Kaiser
//  Copyright (c) 2001-2011 Joel de Guzman
// 
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/spirit/include/karma_char.hpp>
#include <boost/spirit/include/karma_string.hpp>
#include <boost/spirit/include/karma_directive.hpp>
#include <boost/spirit/include/karma_action.hpp>

#include <iostream>
#include "test.hpp"

int
main()
{
    using spirit_test::test;
    using boost::spirit::karma::lit;
    using boost::spirit::karma::lower;
    using boost::spirit::karma::upper;
    using boost::spirit::karma::char_;
    using boost::spirit::karma::encoding;
    namespace char_encoding = boost::spirit::char_encoding;

    encoding<char_encoding::iso8859_1> iso8859_1;

    { // test extended ASCII characters
        BOOST_TEST(test("\xE1", iso8859_1[lower['\xE1']]));
        BOOST_TEST(test("\xC1", iso8859_1[upper['\xE1']]));
        BOOST_TEST(test("\xE1", iso8859_1[lower[char_('\xE1')]]));
        BOOST_TEST(test("\xC1", iso8859_1[upper[char_('\xE1')]]));
        BOOST_TEST(test("\xE1", iso8859_1[lower[lit('\xE1')]]));
        BOOST_TEST(test("\xC1", iso8859_1[upper[lit('\xE1')]]));
        BOOST_TEST(test("\xE1", iso8859_1[lower[char_]], '\xE1'));
        BOOST_TEST(test("\xC1", iso8859_1[upper[char_]], '\xE1'));
        BOOST_TEST(test("\xE1", iso8859_1[lower['\xC1']]));
        BOOST_TEST(test("\xC1", iso8859_1[upper['\xC1']]));
        BOOST_TEST(test("\xE1", iso8859_1[lower[char_('\xC1')]]));
        BOOST_TEST(test("\xC1", iso8859_1[upper[char_('\xC1')]]));
        BOOST_TEST(test("\xE1", iso8859_1[lower[lit('\xC1')]]));
        BOOST_TEST(test("\xC1", iso8859_1[upper[lit('\xC1')]]));
        BOOST_TEST(test("\xE1", iso8859_1[lower[char_]], '\xC1'));
        BOOST_TEST(test("\xC1", iso8859_1[upper[char_]], '\xC1'));

        BOOST_TEST(test("\xE4\xE4", iso8859_1[lower["\xC4\xE4"]]));
        BOOST_TEST(test("\xE4\xE4", iso8859_1[lower[lit("\xC4\xE4")]]));

        BOOST_TEST(test("\xC4\xC4", iso8859_1[upper["\xC4\xE4"]]));
        BOOST_TEST(test("\xC4\xC4", iso8859_1[upper[lit("\xC4\xE4")]]));
    }

    return boost::report_errors();
}
