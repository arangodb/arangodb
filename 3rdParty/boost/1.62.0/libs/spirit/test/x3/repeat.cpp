/*=============================================================================
    Copyright (c) 2001-2011 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <string>
#include <vector>

#include <boost/detail/lightweight_test.hpp>
#include <boost/utility/enable_if.hpp>

#include <boost/spirit/home/x3.hpp>
#include <string>
#include <iostream>
#include "test.hpp"

int
main()
{
    using spirit_test::test_attr;
    using spirit_test::test;

    using namespace boost::spirit::x3::ascii;
    using boost::spirit::x3::repeat;
    using boost::spirit::x3::inf;
    using boost::spirit::x3::omit;
    using boost::spirit::x3::int_;
    using boost::spirit::x3::lexeme;
    using boost::spirit::x3::char_;
    {
        BOOST_TEST(test("aaaaaaaa", repeat[char_])); // kleene synonym
        BOOST_TEST(test("aaaaaaaa", repeat(8)[char_]));
        BOOST_TEST(!test("aa", repeat(3)[char_]));
        BOOST_TEST(test("aaa", repeat(3, 5)[char_]));
        BOOST_TEST(test("aaaaa", repeat(3, 5)[char_]));
        BOOST_TEST(!test("aaaaaa", repeat(3, 5)[char_]));
        BOOST_TEST(!test("aa", repeat(3, 5)[char_]));

        BOOST_TEST(test("aaa", repeat(3, inf)[char_]));
        BOOST_TEST(test("aaaaa", repeat(3, inf)[char_]));
        BOOST_TEST(test("aaaaaa", repeat(3, inf)[char_]));
        BOOST_TEST(!test("aa", repeat(3, inf)[char_]));
    }
    {
        std::string s;
        BOOST_TEST(test_attr("aaaaaaaa", repeat[char_ >> char_], s)); // kleene synonym
        BOOST_TEST(s == "aaaaaaaa");

        s.clear();
        BOOST_TEST(test_attr("aaaaaaaa", repeat(4)[char_ >> char_], s));
        BOOST_TEST(s == "aaaaaaaa");

        BOOST_TEST(!test("aa", repeat(3)[char_ >> char_]));
        BOOST_TEST(!test("a", repeat(1)[char_ >> char_]));

        s.clear();
        BOOST_TEST(test_attr("aa", repeat(1, 3)[char_ >> char_], s));
        BOOST_TEST(s == "aa");

        s.clear();
        BOOST_TEST(test_attr("aaaaaa", repeat(1, 3)[char_ >> char_], s));
        BOOST_TEST(s == "aaaaaa");

        BOOST_TEST(!test("aaaaaaa", repeat(1, 3)[char_ >> char_]));
        BOOST_TEST(!test("a", repeat(1, 3)[char_ >> char_]));

        s.clear();
        BOOST_TEST(test_attr("aaaa", repeat(2, inf)[char_ >> char_], s));
        BOOST_TEST(s == "aaaa");

        s.clear();
        BOOST_TEST(test_attr("aaaaaa", repeat(2, inf)[char_ >> char_], s));
        BOOST_TEST(s == "aaaaaa");

        BOOST_TEST(!test("aa", repeat(2, inf)[char_ >> char_]));
    }

    { // from classic spirit tests
        BOOST_TEST(test("", repeat(0, inf)['x']));

        //  repeat exact 8
        #define rep8 repeat(8)[alpha] >> 'X'
        BOOST_TEST(!test("abcdefgX", rep8, false));
        BOOST_TEST(test("abcdefghX", rep8));
        BOOST_TEST(!test("abcdefghiX", rep8, false));
        BOOST_TEST(!test("abcdefgX", rep8, false));
        BOOST_TEST(!test("aX", rep8, false));

        //  repeat 2 to 8
        #define rep28 repeat(2, 8)[alpha] >> '*'
        BOOST_TEST(test("abcdefg*", rep28));
        BOOST_TEST(test("abcdefgh*", rep28));
        BOOST_TEST(!test("abcdefghi*", rep28, false));
        BOOST_TEST(!test("a*", rep28, false));

        //  repeat 2 or more
        #define rep2_ repeat(2, inf)[alpha] >> '+'
        BOOST_TEST(test("abcdefg+", rep2_));
        BOOST_TEST(test("abcdefgh+", rep2_));
        BOOST_TEST(test("abcdefghi+", rep2_));
        BOOST_TEST(test("abcdefg+", rep2_));
        BOOST_TEST(!test("a+", rep2_, false));

        //  repeat 0
        #define rep0 repeat(0)[alpha] >> '/'
        BOOST_TEST(test("/", rep0));
        BOOST_TEST(!test("a/", rep0, false));

        //  repeat 0 or 1
        #define rep01 repeat(0, 1)[alpha >> digit] >> '?'
        BOOST_TEST(!test("abcdefg?", rep01, false));
        BOOST_TEST(!test("a?", rep01, false));
        BOOST_TEST(!test("1?", rep01, false));
        BOOST_TEST(!test("11?", rep01, false));
        BOOST_TEST(!test("aa?", rep01, false));
        BOOST_TEST(test("?", rep01));
        BOOST_TEST(test("a1?", rep01));
    }

    {
        BOOST_TEST(test(" a a aaa aa", repeat(7)[char_], space));
        BOOST_TEST(test("12345 678 9", repeat(9)[digit], space));
    }

    {
        std::vector<std::string> v;
        BOOST_TEST(test_attr("a b c d", repeat(4)[lexeme[+alpha]], v, space) && 4 == v.size() &&
            v[0] == "a" && v[1] == "b" && v[2] == "c" &&  v[3] == "d");
    }
    {
        BOOST_TEST(test("1 2 3", int_ >> repeat(2)[int_], space));
        BOOST_TEST(!test("1 2", int_ >> repeat(2)[int_], space));
    }

    {
        std::vector<char> v;
        BOOST_TEST(test_attr("1 2 3", int_ >> repeat(2)[int_], v, space));
        BOOST_TEST(v.size() == 3 && v[0] == 1 && v[1] == 2 && v[2] == 3);

        BOOST_TEST(!test("1 2", int_ >> repeat(2)[int_], space));
    }
    return boost::report_errors();
}
