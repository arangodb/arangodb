/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/vector.hpp>
#include <boost/fusion/include/deque.hpp>
#include <boost/fusion/include/at.hpp>
#include <boost/fusion/include/comparison.hpp>

#include <string>
#include <iostream>
#include "test.hpp"
#include "utils.hpp"

int
main()
{
    using boost::spirit::x3::unused_type;

    using boost::spirit::x3::char_;
    using boost::spirit::x3::space;
    using boost::spirit::x3::string;
    using boost::spirit::x3::attr;
    using boost::spirit::x3::omit;
    using boost::spirit::x3::lit;
    using boost::spirit::x3::unused;
    using boost::spirit::x3::int_;
    using boost::spirit::x3::float_;
    using boost::spirit::x3::no_case;
    using boost::spirit::x3::rule;
    using boost::spirit::x3::alnum;

    using boost::spirit::x3::traits::attribute_of;

    using boost::fusion::vector;
    using boost::fusion::deque;
    using boost::fusion::at_c;

    using spirit_test::test;
    using spirit_test::test_attr;

    {
        BOOST_TEST((test("aa", char_ >> char_)));
        BOOST_TEST((test("aa", char_ >> 'a')));
        BOOST_TEST((test("aaa", char_ >> char_ >> char_('a'))));
        BOOST_TEST((test("xi", char_('x') >> char_('i'))));
        BOOST_TEST((!test("xi", char_('x') >> char_('o'))));
        BOOST_TEST((test("xin", char_('x') >> char_('i') >> char_('n'))));
    }

#ifdef BOOST_SPIRIT_COMPILE_ERROR_CHECK
    {
        // Compile check only
        struct x {};
        char_ >> x(); // this should give a reasonable error message
    }
#endif

    {
        BOOST_TEST((test(" a a", char_ >> char_, space)));
        BOOST_TEST((test(" x i", char_('x') >> char_('i'), space)));
        BOOST_TEST((!test(" x i", char_('x') >> char_('o'), space)));
    }


    {
        BOOST_TEST((test(" Hello, World", lit("Hello") >> ',' >> "World", space)));
    }


    {
        vector<char, char> attr;
        BOOST_TEST((test_attr("ab", char_ >> char_, attr)));
        BOOST_TEST((at_c<0>(attr) == 'a'));
        BOOST_TEST((at_c<1>(attr) == 'b'));
    }

#ifdef BOOST_SPIRIT_COMPILE_ERROR_CHECK
    {
        // Compile check only
        vector<char, char> attr;

        // error: attr does not have enough elements
        test_attr("abc", char_ >> char_ >> char_, attr);
    }
#endif

    {
        vector<char, char, char> attr;
        BOOST_TEST((test_attr(" a\n  b\n  c", char_ >> char_ >> char_, attr, space)));
        BOOST_TEST((at_c<0>(attr) == 'a'));
        BOOST_TEST((at_c<1>(attr) == 'b'));
        BOOST_TEST((at_c<2>(attr) == 'c'));
    }

    {
        // 'b' has an unused_type. unused attributes are not part of the sequence
        vector<char, char> attr;
        BOOST_TEST((test_attr("abc", char_ >> 'b' >> char_, attr)));
        BOOST_TEST((at_c<0>(attr) == 'a'));
        BOOST_TEST((at_c<1>(attr) == 'c'));
    }

    {
        // 'b' has an unused_type. unused attributes are not part of the sequence
        vector<char, char> attr;
        BOOST_TEST((test_attr("acb", char_ >> char_ >> 'b', attr)));
        BOOST_TEST((at_c<0>(attr) == 'a'));
        BOOST_TEST((at_c<1>(attr) == 'c'));
    }

    {
        // "hello" has an unused_type. unused attributes are not part of the sequence
        vector<char, char> attr;
        BOOST_TEST((test_attr("a hello c", char_ >> "hello" >> char_, attr, space)));
        BOOST_TEST((at_c<0>(attr) == 'a'));
        BOOST_TEST((at_c<1>(attr) == 'c'));
    }

    {
        // a single element
        char attr;
        BOOST_TEST((test_attr("ab", char_ >> 'b', attr)));
        BOOST_TEST((attr == 'a'));
    }

    {
        // a single element fusion sequence
        vector<char> attr;
        BOOST_TEST((test_attr("ab", char_ >> 'b', attr)));
        BOOST_TEST((at_c<0>(attr) == 'a'));
    }

    {
        // make sure single element tuples get passed through if the rhs
        // has a single element tuple as its attribute. Edit JDG 2014:
        // actually he issue here is that if the rhs in this case a rule
        // (r), it should get it (i.e. the sequence parser should not
        // unwrap it). It's odd that the RHS (r) does not really have a
        // single element tuple (it's a deque<char, int>), so the original
        // comment is not accurate.

        typedef deque<char, int> attr_type;
        attr_type fv;

        auto r = rule<class r, attr_type>()
            = char_ >> ',' >> int_;

        BOOST_TEST((test_attr("test:x,1", "test:" >> r, fv) &&
            fv == attr_type('x', 1)));
    }

    {
        // make sure single element tuples get passed through if the rhs
        // has a single element tuple as its attribute. This is a correction
        // of the test above.

        typedef deque<int> attr_type;
        attr_type fv;

        auto r = rule<class r, attr_type>()
            = int_;

        BOOST_TEST((test_attr("test:1", "test:" >> r, fv) &&
            fv == attr_type(1)));
    }

    {
        // unused means we don't care about the attribute
        BOOST_TEST((test_attr("abc", char_ >> 'b' >> char_, unused)));
    }

    {
        BOOST_TEST((test("aA", no_case[char_('a') >> 'a'])));
        BOOST_TEST((test("BEGIN END", no_case[lit("begin") >> "end"], space)));
        BOOST_TEST((!test("BEGIN END", no_case[lit("begin") >> "nend"], space)));
    }

    { // check attribute is passed through unary to another sequence
        using boost::spirit::x3::eps;
        std::string s;
        BOOST_TEST(test_attr("ab", eps >> no_case[char_ >> char_], s));
        BOOST_TEST("ab" == s);
        s.clear();
        BOOST_TEST(test_attr("ab", no_case[char_ >> char_] >> eps, s));
        BOOST_TEST("ab" == s);
        s.clear();
        BOOST_TEST(test_attr("abc", char_ >> no_case[char_ >> char_], s));
        BOOST_TEST("abc" == s);
        s.clear();
        BOOST_TEST(test_attr("abc", no_case[char_ >> char_] >> char_, s));
        BOOST_TEST("abc" == s);
    }

    {
#ifdef SPIRIT_NO_COMPILE_CHECK
        char_ >> char_ = char_ >> char_; // disallow this!
#endif
    }

    { // alternative forms of attributes. Allow sequences to take in
      // stl containers.

        std::vector<char> v;
        BOOST_TEST(test_attr("abc", char_ >> char_ >> char_, v));
        BOOST_TEST(v.size() == 3);
        BOOST_TEST(v[0] == 'a');
        BOOST_TEST(v[1] == 'b');
        BOOST_TEST(v[2] == 'c');
    }

    { // alternative forms of attributes. Allow sequences to take in
      // stl containers.

        std::vector<char> v;
        BOOST_TEST(test_attr("a,b,c", char_ >> *(',' >> char_), v));
        BOOST_TEST(v.size() == 3);
        BOOST_TEST(v[0] == 'a');
        BOOST_TEST(v[1] == 'b');
        BOOST_TEST(v[2] == 'c');
    }

    { // alternative forms of attributes. Allow sequences to take in
      // stl containers.

        std::vector<char> v;
        BOOST_TEST(test_attr("abc", char_ >> *char_, v));
        BOOST_TEST(v.size() == 3);
        BOOST_TEST(v[0] == 'a');
        BOOST_TEST(v[1] == 'b');
        BOOST_TEST(v[2] == 'c');
    }

    { // alternative forms of attributes. Allow sequences to take in
      // stl containers.
        //~ using boost::spirit::x3::hold;

        std::vector<char> v;
        BOOST_TEST(test_attr("abc", char_ >> *(char_ >> char_), v));
        BOOST_TEST(v.size() == 3);
        BOOST_TEST(v[0] == 'a');
        BOOST_TEST(v[1] == 'b');
        BOOST_TEST(v[2] == 'c');

        v.clear();
        BOOST_TEST(!test_attr("abcd", char_ >> *(char_ >> char_), v));

        // $$$ hold not yet implementd $$$
        //~ v.clear();
        //~ BOOST_TEST(test_attr("abcdef", char_ >> *hold[char_ >> char_] >> char_, v));
        //~ BOOST_TEST(v.size() == 6);
        //~ BOOST_TEST(v[0] == 'a');
        //~ BOOST_TEST(v[1] == 'b');
        //~ BOOST_TEST(v[2] == 'c');
        //~ BOOST_TEST(v[3] == 'd');
        //~ BOOST_TEST(v[4] == 'e');
        //~ BOOST_TEST(v[5] == 'f');

        v.clear();
        BOOST_TEST(test_attr("abc", char_ >> +(char_ >> char_), v));
        BOOST_TEST(v.size() == 3);
        BOOST_TEST(v[0] == 'a');
        BOOST_TEST(v[1] == 'b');
        BOOST_TEST(v[2] == 'c');
    }

    { // alternative forms of attributes. Allow sequences to take in
      // stl containers.

        std::vector<char> v;
        BOOST_TEST(test_attr("abc", char_ >> -(+char_), v));
        BOOST_TEST(v.size() == 3);
        BOOST_TEST(v[0] == 'a');
        BOOST_TEST(v[1] == 'b');
        BOOST_TEST(v[2] == 'c');
    }

    { // alternative forms of attributes. Allow sequences to take in
      // stl containers.

        std::string s;
        BOOST_TEST(test_attr("foobar", string("foo") >> string("bar"), s));
        BOOST_TEST(s == "foobar");

        s.clear();

        // $$$ hold not yet implemented $$$
        //~ using boost::spirit::x3::hold;

        //~ rule<char const*, std::string()> word = +char_("abc");
        //~ BOOST_TEST(test_attr("ab.bc.ca", *hold[word >> string(".")] >> word, s));
        //~ BOOST_TEST(s == "ab.bc.ca");
    }

    // Make sure get_sequence_types works for sequences of sequences.
    {
        std::vector<char> v;
        BOOST_TEST(test_attr(" a b", (' ' >> char_) >> (' ' >> char_), v));
        BOOST_TEST(v.size() == 2);
        BOOST_TEST(v[0] == 'a');
        BOOST_TEST(v[1] == 'b');
    }

    // alternative forms of attributes. Allow sequences to take in
    // stl containers of stl containers.
    {
        std::vector<std::string> v;
        BOOST_TEST(test_attr("abc1,abc2",
            *~char_(',') >> *(',' >> *~char_(',')), v));
        BOOST_TEST(v.size() == 2 && v[0] == "abc1" && v[1] == "abc2");
    }

    {
        std::vector<std::string> v;

        auto e = rule<class e, std::string>()
            = *~char_(',');

        auto l = rule<class l, std::vector<std::string>>()
            = e >> *(',' >> e);

        BOOST_TEST(test_attr("abc1,abc2,abc3", l, v));
        BOOST_TEST(v.size() == 3);
        BOOST_TEST(v[0] == "abc1");
        BOOST_TEST(v[1] == "abc2");
        BOOST_TEST(v[2] == "abc3");
    }

    // do the same with a plain string object
    {
        std::string s;
        BOOST_TEST(test_attr("abc1,abc2",
            *~char_(',') >> *(',' >> *~char_(',')), s));
        BOOST_TEST(s == "abc1abc2");
    }

    {
        std::string s;
        auto e = rule<class e, std::string>()
            = *~char_(',');

        auto l = rule<class l, std::string>()
            = e >> *(',' >> e);

        BOOST_TEST(test_attr("abc1,abc2,abc3", l, s));
        BOOST_TEST(s == "abc1abc2abc3");
    }

    {
        std::vector<char> v;
        BOOST_TEST(test_attr("ab", char_ >> -char_, v));
        BOOST_TEST(v.size() == 2 && v[0] == 'a' && v[1] == 'b');

        v.clear();
        BOOST_TEST(test_attr("a", char_ >> -char_, v));
        BOOST_TEST(v.size() == 1 && v[0] == 'a');

        // $$$ should this be allowed? I don't think so... $$$
        //~ v.clear();
        //~ BOOST_TEST(test_attr("a", char_, v));
        //~ BOOST_TEST(v.size() == 1 && v[0] == 'a');
    }

    {
        std::vector<boost::optional<char>> v;
        BOOST_TEST(test_attr("ab", char_ >> -char_, v));
        BOOST_TEST(v.size() == 2 && v[0] == 'a' && v[1] == 'b');

        v.clear();
        BOOST_TEST(test_attr("a", char_ >> -char_, v));
        BOOST_TEST(v.size() == 2 && v[0] == 'a' && !v[1]);

        // $$$ should this be allowed? I don't think so... $$$
        //~ v.clear();
        //~ BOOST_TEST(test_attr("a", char_, v));
        //~ BOOST_TEST(v.size() == 1 && v[0] == 'a');
    }

    // test from spirit mailing list
    // "Optional operator causes string attribute concatenation"
    {
        typedef vector<char, char, int> attr_type;
        attr_type attr;

        auto node = alnum >> -('[' >> alnum >> '=' >> int_ >> ']');

        BOOST_TEST(test_attr("x[y=123]", node, attr));
        BOOST_TEST(attr == attr_type('x', 'y', 123));
    }

    // test from spirit mailing list (variation of above)
    // "Optional operator causes string attribute concatenation"
    {
        typedef vector<std::string, std::string, int> attr_type;
        attr_type attr;

        auto node = +alnum >> -('[' >> +alnum >> '=' >> int_ >> ']');

        BOOST_TEST(test_attr("xxx[yyy=123]", node, attr));
        BOOST_TEST(attr == attr_type("xxx", "yyy", 123));
    }

    // test from spirit mailing list
    // "Error with container within sequence"
    {
        typedef vector<std::string> attr_type;
        attr_type attr;

        auto r = *alnum;

        BOOST_TEST(test_attr("abcdef", r, attr));
        BOOST_TEST(at_c<0>(attr) == "abcdef");
    }

    // test from spirit mailing list (variation of above)
    // "Error with container within sequence"
    {
        typedef vector<std::vector<int>> attr_type;
        attr_type attr;

        auto r = *int_;

        BOOST_TEST(test_attr("123 456", r, attr, space));
        BOOST_TEST(at_c<0>(attr).size() == 2);
        BOOST_TEST(at_c<0>(attr)[0] == 123);
        BOOST_TEST(at_c<0>(attr)[1] == 456);
    }

    {
        using Attr = boost::variant<int, float>;
        Attr attr;
        auto const term = rule<class term, Attr>("term") = int_ | float_;
        auto const expr = rule<class expr, Attr>("expr") = term | ('(' > term > ')');
        BOOST_TEST((test_attr("(1)", expr, attr, space)));
    }

    // test that failing sequence leaves attribute consistent
    {
	std::string attr;
	//no need to use omit[], but lit() is buggy ATM
	BOOST_TEST(test_attr("A\nB\nC", *(char_ >> omit[lit("\n")]), attr, false));
	BOOST_TEST(attr == "AB");
    }

    // test that sequence with only one parser producing attribute
    // makes it unwrapped
    {
	BOOST_TEST((boost::is_same<
		    typename attribute_of<decltype(lit("abc") >> attr(long())), unused_type>::type,
		    long>() ));
    }

    {   // test action
        using boost::fusion::at_c;

        char c = 0;
        int n = 0;
        auto f = [&](auto& ctx)
            {
                c = at_c<0>(_attr(ctx));
                n = at_c<1>(_attr(ctx));
            };

        BOOST_TEST(test("x123\"a string\"", (char_ >> int_ >> "\"a string\"")[f]));
        BOOST_TEST(c == 'x');
        BOOST_TEST(n == 123);
    }

    {   // test action
        char c = 0;
        int n = 0;
        auto f = [&](auto& ctx)
            {
                c = at_c<0>(_attr(ctx));
                n = at_c<1>(_attr(ctx));
            };

        BOOST_TEST(test("x 123 \"a string\"", (char_ >> int_ >> "\"a string\"")[f], space));
        BOOST_TEST(c == 'x');
        BOOST_TEST(n == 123);
    }

    {
#ifdef SPIRIT_NO_COMPILE_CHECK
        char const* const s = "";
        int i;
        parse(s, s, int_ >> int_, i);
#endif
    }

    { // test move only types
        using boost::spirit::x3::eps;
        std::vector<move_only> v;
        BOOST_TEST(test_attr("ssszs", *synth_move_only >> 'z' >> synth_move_only, v));
        BOOST_TEST_EQ(v.size(), 4);
    }

    return boost::report_errors();
}
