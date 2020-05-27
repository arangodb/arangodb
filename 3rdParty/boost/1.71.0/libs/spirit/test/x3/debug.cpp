/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#define BOOST_SPIRIT_X3_DEBUG

#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/fusion/include/vector.hpp>

#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include "test.hpp"

struct my_error_handler
{
    template <typename Iterator, typename Exception, typename Context>
    boost::spirit::x3::error_handler_result
    operator()(Iterator&, Iterator const& last, Exception const& x, Context const&) const
    {
        std::cout
            << "Error! Expecting: "
            << x.which()
            << ", got: \""
            << std::string(x.where(), last)
            << "\""
            << std::endl;
        return boost::spirit::x3::error_handler_result::fail;
    }
};

struct my_attribute
{
    bool alive = true;

    void access() const
    {
        BOOST_TEST(alive);
    }
    ~my_attribute()
    {
        alive = false;
    }

    friend std::ostream & operator << (std::ostream & os, my_attribute const & attr)
    {
        attr.access();
        return os << "my_attribute";
    }
};

int
main()
{
    using spirit_test::test_attr;
    using spirit_test::test;

    using namespace boost::spirit::x3::ascii;
    using boost::spirit::x3::rule;
    using boost::spirit::x3::symbols;
    using boost::spirit::x3::int_;
    using boost::spirit::x3::alpha;

    { // basic tests

        auto a = rule<class a>("a") = 'a';
        auto b = rule<class b>("b") = 'b';
        auto c = rule<class c>("c") = 'c';

        {
            auto start = *(a | b | c);
            BOOST_TEST(test("abcabcacb", start));
        }

        {
            rule<class start> start("start");
            auto start_def =
                start = (a | b) >> (start | b);

            BOOST_TEST(test("aaaabababaaabbb", start_def));
            BOOST_TEST(test("aaaabababaaabba", start_def, false));
        }
    }

    { // basic tests w/ skipper

        auto a = rule<class a>("a") = 'a';
        auto b = rule<class b>("b") = 'b';
        auto c = rule<class c>("c") = 'c';

        {
            auto start = *(a | b | c);
            BOOST_TEST(test(" a b c a b c a c b ", start, space));
        }

        {
            rule<class start> start("start");
            auto start_def =
                start = (a | b) >> (start | b);

            BOOST_TEST(test(" a a a a b a b a b a a a b b b ", start_def, space));
            BOOST_TEST(test(" a a a a b a b a b a a a b b a ", start_def, space, false));
        }
    }

    { // std::container attributes

        typedef boost::fusion::vector<int, char> fs;
        rule<class start, std::vector<fs>> start("start");
        auto start_def =
            start = *(int_ >> alpha);

        BOOST_TEST(test("1 a 2 b 3 c", start_def, space));
    }

    { // error handling

        auto r_def = '(' > int_ > ',' > int_ > ')';
        auto r = r_def.on_error(my_error_handler());

        BOOST_TEST(test("(123,456)", r));
        BOOST_TEST(!test("(abc,def)", r));
        BOOST_TEST(!test("(123,456]", r));
        BOOST_TEST(!test("(123;456)", r));
        BOOST_TEST(!test("[123,456]", r));
    }

    {
        symbols<my_attribute> a{{{ "a", my_attribute{} }}};

        auto b = rule<struct b, my_attribute>("b") = a;

        my_attribute attr;

        BOOST_TEST(test_attr("a", b, attr));
    }

    return boost::report_errors();
}
