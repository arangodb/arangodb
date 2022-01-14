/*=============================================================================
    Copyright (c) 2001-2013 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <string>
#include <vector>
#include <set>
#include <map>

#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>

#include <string>
#include <iostream>
#include "test.hpp"
#include "utils.hpp"

using namespace spirit_test;

int
main()
{
    using namespace boost::spirit::x3::ascii;

    BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(char_ % ',');

    {
        BOOST_TEST(test("a,b,c,d,e,f,g,h", char_ % ','));
        BOOST_TEST(test("a,b,c,d,e,f,g,h,", char_ % ',', false));
    }

    {
        BOOST_TEST(test("a, b, c, d, e, f, g, h", char_ % ',', space));
        BOOST_TEST(test("a, b, c, d, e, f, g, h,", char_ % ',', space, false));
    }

    {
        std::string s;
        BOOST_TEST(test_attr("a,b,c,d,e,f,g,h", char_ % ',', s));
        BOOST_TEST(s == "abcdefgh");

        BOOST_TEST(!test("a,b,c,d,e,f,g,h,", char_ % ','));
    }

    {
        std::string s;
        BOOST_TEST(test_attr("ab,cd,ef,gh", (char_ >> char_) % ',', s));
        BOOST_TEST(s == "abcdefgh");

        BOOST_TEST(!test("ab,cd,ef,gh,", (char_ >> char_) % ','));
        BOOST_TEST(!test("ab,cd,ef,g", (char_ >> char_) % ','));

        s.clear();
        BOOST_TEST(test_attr("ab,cd,efg", (char_ >> char_) % ',' >> char_, s));
        BOOST_TEST(s == "abcdefg");
    }

    { // regression test for has_attribute
        using boost::spirit::x3::int_;
        using boost::spirit::x3::omit;

        int i;
        BOOST_TEST(test_attr("1:2,3", int_ >> ':' >> omit[int_] % ',', i))
          && BOOST_TEST_EQ(i, 1);
    }

    {
        using boost::spirit::x3::int_;

        std::vector<int> v;
        BOOST_TEST(test_attr("1,2", int_ % ',', v));
        BOOST_TEST(2 == v.size() && 1 == v[0] && 2 == v[1]);
    }

    {
        using boost::spirit::x3::int_;

        std::vector<int> v;
        BOOST_TEST(test_attr("(1,2)", '(' >> int_ % ',' >> ')', v));
        BOOST_TEST(2 == v.size() && 1 == v[0] && 2 == v[1]);
    }

    {
        std::vector<std::string> v;
        BOOST_TEST(test_attr("a,b,c,d", +alpha % ',', v));
        BOOST_TEST(4 == v.size() && "a" == v[0] && "b" == v[1]
            && "c" == v[2] && "d" == v[3]);
    }

    {
        std::vector<boost::optional<char> > v;
        BOOST_TEST(test_attr("#a,#", ('#' >> -alpha) % ',', v));
        BOOST_TEST(2 == v.size() &&
            !!v[0] && 'a' == boost::get<char>(v[0]) && !v[1]);

        std::vector<char> v2;
        BOOST_TEST(test_attr("#a,#", ('#' >> -alpha) % ',', v2));
        BOOST_TEST(1 == v2.size() && 'a' == v2[0]);
    }

    { // actions
        using boost::spirit::x3::_attr;

        std::string s;
        auto f = [&](auto& ctx){ s = std::string(_attr(ctx).begin(), _attr(ctx).end()); };

        BOOST_TEST(test("a,b,c,d,e,f,g,h", (char_ % ',')[f]));
        BOOST_TEST(s == "abcdefgh");
    }

    { // test move only types
        std::vector<move_only> v;
        BOOST_TEST(test_attr("s.s.s.s", synth_move_only % '.', v));
        BOOST_TEST_EQ(v.size(), 4);
    }

    return boost::report_errors();
}
