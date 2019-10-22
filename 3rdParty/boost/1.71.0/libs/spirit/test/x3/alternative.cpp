/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman
    Copyright (c) 2001-2011 Hartmut Kaiser

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/variant.hpp>
#include <boost/fusion/include/vector.hpp>
#include <boost/fusion/include/at.hpp>

#include <string>
#include <iostream>
#include <vector>
#include "test.hpp"

struct di_ignore
{
    std::string text;
};

struct di_include
{
    std::string FileName;
};

BOOST_FUSION_ADAPT_STRUCT(di_ignore,
    text
)

BOOST_FUSION_ADAPT_STRUCT(di_include,
    FileName
)

struct undefined {};


struct stationary : boost::noncopyable
{
    explicit stationary(int i) : val{i} {}
    // TODO: fix unneeded self move in alternative
    stationary& operator=(stationary&&) { std::abort(); }
    stationary& operator=(int i) { val = i; return *this; }

    int val;
};


int
main()
{
    using spirit_test::test;
    using spirit_test::test_attr;

    using boost::spirit::x3::attr;
    using boost::spirit::x3::char_;
    using boost::spirit::x3::int_;
    using boost::spirit::x3::lit;
    using boost::spirit::x3::unused_type;
    using boost::spirit::x3::unused;
    using boost::spirit::x3::omit;
    using boost::spirit::x3::eps;


    {
        BOOST_TEST((test("a", char_ | char_)));
        BOOST_TEST((test("x", lit('x') | lit('i'))));
        BOOST_TEST((test("i", lit('x') | lit('i'))));
        BOOST_TEST((!test("z", lit('x') | lit('o'))));
        BOOST_TEST((test("rock", lit("rock") | lit("roll"))));
        BOOST_TEST((test("roll", lit("rock") | lit("roll"))));
        BOOST_TEST((test("rock", lit("rock") | int_)));
        BOOST_TEST((test("12345", lit("rock") | int_)));
    }

    {
        typedef boost::variant<undefined, int, char> attr_type;
        attr_type v;

        BOOST_TEST((test_attr("12345", int_ | char_, v)));
        BOOST_TEST(boost::get<int>(v) == 12345);

        BOOST_TEST((test_attr("12345", lit("rock") | int_ | char_, v)));
        BOOST_TEST(boost::get<int>(v) == 12345);

        v = attr_type();
        BOOST_TEST((test_attr("rock", lit("rock") | int_ | char_, v)));
        BOOST_TEST(v.which() == 0);

        BOOST_TEST((test_attr("x", lit("rock") | int_ | char_, v)));
        BOOST_TEST(boost::get<char>(v) == 'x');
    }

    {   // Make sure that we are using the actual supplied attribute types
        // from the variant and not the expected type.
        boost::variant<int, std::string> v;
        BOOST_TEST((test_attr("12345", int_ | +char_, v)));
        BOOST_TEST(boost::get<int>(v) == 12345);

        BOOST_TEST((test_attr("abc", int_ | +char_, v)));
        BOOST_TEST(boost::get<std::string>(v) == "abc");

        BOOST_TEST((test_attr("12345", +char_ | int_, v)));
        BOOST_TEST(boost::get<std::string>(v) == "12345");
    }

    {
        unused_type x;
        BOOST_TEST((test_attr("rock", lit("rock") | lit('x'), x)));
    }

    {
        // test if alternatives with all components having unused
        // attributes have an unused attribute

        using boost::fusion::vector;
        using boost::fusion::at_c;

        vector<char, char> v;
        BOOST_TEST((test_attr("abc",
            char_ >> (omit[char_] | omit[char_]) >> char_, v)));
        BOOST_TEST((at_c<0>(v) == 'a'));
        BOOST_TEST((at_c<1>(v) == 'c'));
    }

    {
        // Test that we can still pass a "compatible" attribute to
        // an alternate even if its "expected" attribute is unused type.

        std::string s;
        BOOST_TEST((test_attr("...", *(char_('.') | char_(',')), s)));
        BOOST_TEST(s == "...");
    }

    {   // make sure collapsing eps works as expected
        // (compile check only)

        using boost::spirit::x3::rule;
        using boost::spirit::x3::eps;
        using boost::spirit::x3::_attr;
        using boost::spirit::x3::_val;

        rule<class r1, wchar_t> r1;
        rule<class r2, wchar_t> r2;
        rule<class r3, wchar_t> r3;
        
        auto f = [&](auto& ctx){ _val(ctx) = _attr(ctx); };

        r3  = ((eps >> r1))[f];
        r3  = ((r1) | r2)[f];
        r3 = ((eps >> r1) | r2);
    }

    {
        std::string s;
        using boost::spirit::x3::eps;

        // test having a variant<container, ...>
        BOOST_TEST( (test_attr("a,b", (char_ % ',') | eps, s )) );
        BOOST_TEST(s == "ab");
    }

    {
        using boost::spirit::x3::eps;

        // testing a sequence taking a container as attribute
        std::string s;
        BOOST_TEST( (test_attr("abc,a,b,c",
            char_ >> char_ >> (char_ % ','), s )) );
        BOOST_TEST(s == "abcabc");

        // test having an optional<container> inside a sequence
        s.erase();
        BOOST_TEST( (test_attr("ab",
            char_ >> char_ >> -(char_ % ','), s )) );
        BOOST_TEST(s == "ab");

        // test having a variant<container, ...> inside a sequence
        s.erase();
        BOOST_TEST( (test_attr("ab",
            char_ >> char_ >> ((char_ % ',') | eps), s )) );
        BOOST_TEST(s == "ab");
        s.erase();
        BOOST_TEST( (test_attr("abc",
            char_ >> char_ >> ((char_ % ',') | eps), s )) );
        BOOST_TEST(s == "abc");
    }

    {
        //compile test only (bug_march_10_2011_8_35_am)
        typedef boost::variant<double, std::string> value_type;

        using boost::spirit::x3::rule;
        using boost::spirit::x3::eps;

        rule<class r1, value_type> r1;
        auto r1_ = r1 = r1 | eps; // left recursive!

        unused = r1_; // silence unused local warning
    }

    {
        using boost::spirit::x3::rule;
        typedef boost::variant<di_ignore, di_include> d_line;

        rule<class ignore, di_ignore> ignore;
        rule<class include, di_include> include;
        rule<class line, d_line> line;

        auto start =
            line = include | ignore;

        unused = start; // silence unused local warning
    }

    // single-element fusion vector tests
    {
        boost::fusion::vector<boost::variant<int, std::string>> fv;
        BOOST_TEST((test_attr("12345", int_ | +char_, fv)));
        BOOST_TEST(boost::get<int>(boost::fusion::at_c<0>(fv)) == 12345);

        boost::fusion::vector<boost::variant<int, std::string>> fvi;
        BOOST_TEST((test_attr("12345", int_ | int_, fvi)));
        BOOST_TEST(boost::get<int>(boost::fusion::at_c<0>(fvi)) == 12345);
    }

    // alternative over single element sequences as part of another sequence
    {
        auto  key1 = lit("long") >> attr(long());
        auto  key2 = lit("char") >> attr(char());
        auto  keys = key1 | key2;
        auto pair = keys >> lit("=") >> +char_;

        boost::fusion::deque<boost::variant<long, char>, std::string> attr_;

        BOOST_TEST(test_attr("long=ABC", pair, attr_));
        BOOST_TEST(boost::get<long>(&boost::fusion::front(attr_)) != nullptr);
        BOOST_TEST(boost::get<char>(&boost::fusion::front(attr_)) == nullptr);
    }

    { // ensure no unneded synthesization, copying and moving occured
        auto p = '{' >> int_ >> '}';

        stationary st { 0 };
        BOOST_TEST(test_attr("{42}", p | eps | p, st));
        BOOST_TEST_EQ(st.val, 42);
    }

    return boost::report_errors();
}
