/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <string>
#include <vector>

#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>

#include <string>
#include <iostream>
#include "test.hpp"
#include "utils.hpp"

struct x_attr
{
};

namespace boost { namespace spirit { namespace x3 { namespace traits
{
    template <>
    struct container_value<x_attr>
    {
        typedef char type; // value type of container
    };

    template <>
    struct push_back_container<x_attr>
    {
        static bool call(x_attr& /*c*/, char /*val*/)
        {
            // push back value type into container
            return true;
        }
    };
}}}}

int
main()
{
    using spirit_test::test;
    using spirit_test::test_attr;
    using boost::spirit::x3::char_;
    using boost::spirit::x3::alpha;
    using boost::spirit::x3::upper;
    using boost::spirit::x3::space;
    using boost::spirit::x3::digit;
    using boost::spirit::x3::int_;
    using boost::spirit::x3::lexeme;

    {
        BOOST_TEST(test("aaaaaaaa", *char_));
        BOOST_TEST(test("a", *char_));
        BOOST_TEST(test("", *char_));
        BOOST_TEST(test("aaaaaaaa", *alpha));
        BOOST_TEST(!test("aaaaaaaa", *upper));
    }

    {
        BOOST_TEST(test(" a a aaa aa", *char_, space));
        BOOST_TEST(test("12345 678 9", *digit, space));
    }

    {
        std::string s;
        BOOST_TEST(test_attr("bbbb", *char_, s) && 4 == s.size() && s == "bbbb");

        s.clear();
        BOOST_TEST(test_attr("b b b b ", *char_, s, space)  && s == "bbbb");
    }

    {
        std::vector<int> v;
        BOOST_TEST(test_attr("123 456 789 10", *int_, v, space) && 4 == v.size() &&
            v[0] == 123 && v[1] == 456 && v[2] == 789 &&  v[3] == 10);
    }

    {
        std::vector<std::string> v;
        BOOST_TEST(test_attr("a b c d", *lexeme[+alpha], v, space) && 4 == v.size() &&
            v[0] == "a" && v[1] == "b" && v[2] == "c" &&  v[3] == "d");
    }

    {
        std::vector<int> v;
        BOOST_TEST(test_attr("123 456 789", *int_, v, space) && 3 == v.size() &&
            v[0] == 123 && v[1] == 456 && v[2] == 789);
    }

    { // actions
        using boost::spirit::x3::_attr;

        std::string v;
        auto f = [&](auto& ctx){ v = _attr(ctx); };

        BOOST_TEST(test("bbbb", (*char_)[f]) && 4 == v.size() &&
            v[0] == 'b' && v[1] == 'b' && v[2] == 'b' &&  v[3] == 'b');
    }

    { // more actions
        using boost::spirit::x3::_attr;

        std::vector<int> v;
        auto f = [&](auto& ctx){ v = _attr(ctx); };

        BOOST_TEST(test("123 456 789", (*int_)[f], space) && 3 == v.size() &&
            v[0] == 123 && v[1] == 456 && v[2] == 789);
    }

    { // attribute customization

        x_attr x;
        test_attr("abcde", *char_, x);
    }

    { // test move only types
        std::vector<move_only> v;
        BOOST_TEST(test_attr("sss", *synth_move_only, v));
        BOOST_TEST_EQ(v.size(), 3);
    }

    return boost::report_errors();
}
