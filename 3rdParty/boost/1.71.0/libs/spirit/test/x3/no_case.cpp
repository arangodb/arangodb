/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman
    http://spirit.sourceforge.net/

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>

#include <iostream>
#include "test.hpp"

int
main()
{
    using spirit_test::test;
    using spirit_test::test_attr;
    using boost::spirit::x3::no_case;

    {
        using namespace boost::spirit::x3::ascii;
        BOOST_TEST(test("x", no_case[char_]));
        BOOST_TEST(test("X", no_case[char_('x')]));
        BOOST_TEST(test("X", no_case[char_('X')]));
        BOOST_TEST(test("x", no_case[char_('X')]));
        BOOST_TEST(test("x", no_case[char_('x')]));
        BOOST_TEST(!test("z", no_case[char_('X')]));
        BOOST_TEST(!test("z", no_case[char_('x')]));
        BOOST_TEST(test("x", no_case[char_('a', 'z')]));
        BOOST_TEST(test("X", no_case[char_('a', 'z')]));
        BOOST_TEST(!test("a", no_case[char_('b', 'z')]));
        BOOST_TEST(!test("z", no_case[char_('a', 'y')]));
    }
    {
        using namespace boost::spirit::x3::ascii;
        BOOST_TEST(test("X", no_case['x']));
        BOOST_TEST(test("X", no_case['X']));
        BOOST_TEST(test("x", no_case['X']));
        BOOST_TEST(test("x", no_case['x']));
        BOOST_TEST(!test("z", no_case['X']));
        BOOST_TEST(!test("z", no_case['x']));
    }

    {
        using namespace boost::spirit::x3::iso8859_1;
        BOOST_TEST(test("X", no_case[char_("a-z")]));
        BOOST_TEST(!test("1", no_case[char_("a-z")]));
    }

    { // test extended ASCII characters
        using namespace boost::spirit::x3::iso8859_1;
        BOOST_TEST(test("\xC1", no_case[char_('\xE1')]));

        BOOST_TEST(test("\xC9", no_case[char_("\xE5-\xEF")]));
        BOOST_TEST(!test("\xFF", no_case[char_("\xE5-\xEF")]));

        BOOST_TEST(test("\xC1\xE1", no_case[lit("\xE1\xC1")]));
        BOOST_TEST(test("\xE1\xE1", no_case[no_case[lit("\xE1\xC1")]]));
    }

    {
        using namespace boost::spirit::x3::ascii;
        BOOST_TEST(test("Bochi Bochi", no_case[lit("bochi bochi")]));
        BOOST_TEST(test("BOCHI BOCHI", no_case[lit("bochi bochi")]));
        BOOST_TEST(!test("Vavoo", no_case[lit("bochi bochi")]));
    }

    {
        // should work!
        using namespace boost::spirit::x3::ascii;
        BOOST_TEST(test("x", no_case[no_case[char_]]));
        BOOST_TEST(test("x", no_case[no_case[char_('x')]]));
        BOOST_TEST(test("yabadabadoo", no_case[no_case[lit("Yabadabadoo")]]));
    }

    {
        using namespace boost::spirit::x3::ascii;
        BOOST_TEST(test("X", no_case[alnum]));
        BOOST_TEST(test("6", no_case[alnum]));
        BOOST_TEST(!test(":", no_case[alnum]));

        BOOST_TEST(test("X", no_case[lower]));
        BOOST_TEST(test("x", no_case[lower]));
        BOOST_TEST(test("X", no_case[upper]));
        BOOST_TEST(test("x", no_case[upper]));
        BOOST_TEST(!test(":", no_case[lower]));
        BOOST_TEST(!test(":", no_case[upper]));
    }

    {
        using namespace boost::spirit::x3::iso8859_1;
        BOOST_TEST(test("X", no_case[alnum]));
        BOOST_TEST(test("6", no_case[alnum]));
        BOOST_TEST(!test(":", no_case[alnum]));

        BOOST_TEST(test("X", no_case[lower]));
        BOOST_TEST(test("x", no_case[lower]));
        BOOST_TEST(test("X", no_case[upper]));
        BOOST_TEST(test("x", no_case[upper]));
        BOOST_TEST(!test(":", no_case[lower]));
        BOOST_TEST(!test(":", no_case[upper]));
    }

    {
        using namespace boost::spirit::x3::standard;
        BOOST_TEST(test("X", no_case[alnum]));
        BOOST_TEST(test("6", no_case[alnum]));
        BOOST_TEST(!test(":", no_case[alnum]));

        BOOST_TEST(test("X", no_case[lower]));
        BOOST_TEST(test("x", no_case[lower]));
        BOOST_TEST(test("X", no_case[upper]));
        BOOST_TEST(test("x", no_case[upper]));
        BOOST_TEST(!test(":", no_case[lower]));
        BOOST_TEST(!test(":", no_case[upper]));
    }

    {
        // chsets
        namespace standard = boost::spirit::x3::standard;
        namespace standard_wide = boost::spirit::x3::standard_wide;

        BOOST_TEST(test("x", no_case[standard::char_("a-z")]));
        BOOST_TEST(test("X", no_case[standard::char_("a-z")]));
        BOOST_TEST(test(L"X", no_case[standard_wide::char_(L"a-z")]));
        BOOST_TEST(test(L"X", no_case[standard_wide::char_(L"X")]));
    }

    {
        using namespace boost::spirit::x3::standard;
        std::string s("bochi bochi");
        BOOST_TEST(test("Bochi Bochi", no_case[lit(s.c_str())]));
        BOOST_TEST(test("Bochi Bochi", no_case[lit(s)]));
        BOOST_TEST(test("Bochi Bochi", no_case[s.c_str()]));
        BOOST_TEST(test("Bochi Bochi", no_case[s]));
    }

    return boost::report_errors();
}
