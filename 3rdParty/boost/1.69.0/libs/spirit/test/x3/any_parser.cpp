/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman
    Copyright (c) 2013-2014 Agustin Berge

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

// this file deliberately contains non-ascii characters
// boostinspect:noascii

#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>

#include <string>
#include <cstring>
#include <iostream>
#include "test.hpp"

int
main()
{
    using spirit_test::test_attr;
    using spirit_test::test;

    using namespace boost::spirit::x3::ascii;
    using boost::spirit::x3::any_parser;
    using boost::spirit::x3::int_;
    using boost::spirit::x3::make_context;
    using boost::spirit::x3::lit;
    using boost::spirit::x3::unused_type;
    using boost::spirit::x3::phrase_parse;
    using boost::spirit::x3::skip_flag;
    using boost::spirit::x3::skipper_tag;
    using boost::spirit::x3::_attr;

    typedef char const* iterator_type;
    typedef decltype(make_context<skipper_tag>(space)) context_type;
    { // basic tests

        auto a = lit('a');
        auto b = lit('b');
        auto c = lit('c');

        {
            any_parser<iterator_type> start =
                *(a | b | c);

            BOOST_TEST(test("abcabcacb", start));
        }
    }

    { // basic tests w/ skipper

        auto a = lit('a');
        auto b = lit('b');
        auto c = lit('c');

        {
            any_parser<iterator_type, unused_type, context_type> start =
                *(a | b | c);

            BOOST_TEST(test(" a b c a b c a c b ", start, space));
        }
    }

    { // basic tests w/ skipper but no final post-skip

        any_parser<iterator_type, unused_type, context_type> a = lit('a');
        any_parser<iterator_type, unused_type, context_type> b = lit('b');
        any_parser<iterator_type, unused_type, context_type> c = lit('c');

        {
            any_parser<iterator_type, unused_type, context_type> start = *(a | b) >> c;

            char const *s1 = " a b a a b b a c ... "
              , *const e1 = s1 + std::strlen(s1);
            BOOST_TEST(phrase_parse(s1, e1, start, space, skip_flag::dont_post_skip)
              && s1 == e1 - 5);
        }
    }

    { // context tests

        char ch;
        any_parser<iterator_type, char> a = alpha;

        // this semantic action requires both the context and attribute
        //!!auto f = [&](auto&, char attr){ ch = attr; };
        //!!BOOST_TEST(test("x", a[f]));
        //!!BOOST_TEST(ch == 'x');

        // the semantic action may have the context passed
        auto f2 = [&](auto&){ ch = 'y'; };
        BOOST_TEST(test("x", a[f2]));
        BOOST_TEST(ch == 'y');

        // the semantic action may optionally not have any arguments at all
        auto f3 = [&]{ ch = 'z'; };
        BOOST_TEST(test("x", a[f3]));
        BOOST_TEST(ch == 'z');

        BOOST_TEST(test_attr("z", a, ch)); // attribute is given.
        BOOST_TEST(ch == 'z');
    }

    { // auto rules tests

        char ch = '\0';
        any_parser<iterator_type, char> a = alpha;
        auto f = [&](auto& ctx){ ch = _attr(ctx); };

        BOOST_TEST(test("x", a[f]));
        BOOST_TEST(ch == 'x');
        ch = '\0';
        BOOST_TEST(test_attr("z", a, ch)); // attribute is given.
        BOOST_TEST(ch == 'z');

        ch = '\0';
        BOOST_TEST(test("x", a[f]));
        BOOST_TEST(ch == 'x');
        ch = '\0';
        BOOST_TEST(test_attr("z", a, ch)); // attribute is given.
        BOOST_TEST(ch == 'z');
    }

    { // auto rules tests: allow stl containers as attributes to
      // sequences (in cases where attributes of the elements
      // are convertible to the value_type of the container or if
      // the element itself is an stl container with value_type
      // that is convertible to the value_type of the attribute).

        std::string s;
        auto f = [&](auto& ctx){ s = _attr(ctx); };

        {
            any_parser<iterator_type, std::string> r
                = char_ >> *(',' >> char_)
                ;

            BOOST_TEST(test("a,b,c,d,e,f", r[f]));
            BOOST_TEST(s == "abcdef");
        }

        {
            any_parser<iterator_type, std::string> r
                = char_ >> *(',' >> char_);
            s.clear();
            BOOST_TEST(test("a,b,c,d,e,f", r[f]));
            BOOST_TEST(s == "abcdef");
        }

        {
            any_parser<iterator_type, std::string> r
                = char_ >> char_ >> char_ >> char_ >> char_ >> char_;
            s.clear();
            BOOST_TEST(test("abcdef", r[f]));
            BOOST_TEST(s == "abcdef");
        }
    }

    return boost::report_errors();
}
