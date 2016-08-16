/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman
    Copyright (c) 2001-2011 Hartmut Kaiser

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3/core.hpp>
#include <boost/spirit/home/x3/char.hpp>

#include <iostream>
#include "test.hpp"

int
main()
{
    using spirit_test::test;

    {
        using namespace boost::spirit::x3::ascii;

        BOOST_TEST(test("x", 'x'));
        BOOST_TEST(test(L"x", L'x'));
        BOOST_TEST(!test("y", 'x'));
        BOOST_TEST(!test(L"y", L'x'));

        BOOST_TEST(test("x", char_));
        BOOST_TEST(test("x", char_('x')));
        BOOST_TEST(!test("x", char_('y')));
        BOOST_TEST(test("x", char_('a', 'z')));
        BOOST_TEST(!test("x", char_('0', '9')));

        BOOST_TEST(test("0", char_('0', '9')));
        BOOST_TEST(test("9", char_('0', '9')));
        BOOST_TEST(!test("0", ~char_('0', '9')));
        BOOST_TEST(!test("9", ~char_('0', '9')));

        BOOST_TEST(!test("x", ~char_));
        BOOST_TEST(!test("x", ~char_('x')));
        BOOST_TEST(test(" ", ~char_('x')));
        BOOST_TEST(test("X", ~char_('x')));
        BOOST_TEST(!test("x", ~char_('b', 'y')));
        BOOST_TEST(test("a", ~char_('b', 'y')));
        BOOST_TEST(test("z", ~char_('b', 'y')));

        BOOST_TEST(test("x", ~~char_));
        BOOST_TEST(test("x", ~~char_('x')));
        BOOST_TEST(!test(" ", ~~char_('x')));
        BOOST_TEST(!test("X", ~~char_('x')));
        BOOST_TEST(test("x", ~~char_('b', 'y')));
        BOOST_TEST(!test("a", ~~char_('b', 'y')));
        BOOST_TEST(!test("z", ~~char_('b', 'y')));
    }

    {
        using namespace boost::spirit::x3::ascii;

        BOOST_TEST(test("   x", 'x', space));
        BOOST_TEST(test(L"   x", L'x', space));

        BOOST_TEST(test("   x", char_, space));
        BOOST_TEST(test("   x", char_('x'), space));
        BOOST_TEST(!test("   x", char_('y'), space));
        BOOST_TEST(test("   x", char_('a', 'z'), space));
        BOOST_TEST(!test("   x", char_('0', '9'), space));
    }

    {
        using namespace boost::spirit::x3::standard_wide;

        BOOST_TEST(test(L"x", char_));
        BOOST_TEST(test(L"x", char_(L'x')));
        BOOST_TEST(!test(L"x", char_(L'y')));
        BOOST_TEST(test(L"x", char_(L'a', L'z')));
        BOOST_TEST(!test(L"x", char_(L'0', L'9')));

        BOOST_TEST(!test(L"x", ~char_));
        BOOST_TEST(!test(L"x", ~char_(L'x')));
        BOOST_TEST(test(L" ", ~char_(L'x')));
        BOOST_TEST(test(L"X", ~char_(L'x')));
        BOOST_TEST(!test(L"x", ~char_(L'b', L'y')));
        BOOST_TEST(test(L"a", ~char_(L'b', L'y')));
        BOOST_TEST(test(L"z", ~char_(L'b', L'y')));

        BOOST_TEST(test(L"x", ~~char_));
        BOOST_TEST(test(L"x", ~~char_(L'x')));
        BOOST_TEST(!test(L" ", ~~char_(L'x')));
        BOOST_TEST(!test(L"X", ~~char_(L'x')));
        BOOST_TEST(test(L"x", ~~char_(L'b', L'y')));
        BOOST_TEST(!test(L"a", ~~char_(L'b', L'y')));
        BOOST_TEST(!test(L"z", ~~char_(L'b', L'y')));
    }


    {   // single char strings!
        namespace ascii = boost::spirit::x3::ascii;
         namespace wide = boost::spirit::x3::standard_wide;

        BOOST_TEST(test("x", "x"));
        BOOST_TEST(test(L"x", L"x"));
        BOOST_TEST(test("x", ascii::char_("x")));
        BOOST_TEST(test(L"x", wide::char_(L"x")));

        BOOST_TEST(test("x", ascii::char_("a", "z")));
        BOOST_TEST(test(L"x", ascii::char_(L"a", L"z")));
    }

    {
        // chsets
        namespace ascii = boost::spirit::x3::ascii;
        namespace wide = boost::spirit::x3::standard_wide;

        BOOST_TEST(test("x", ascii::char_("a-z")));
        BOOST_TEST(!test("1", ascii::char_("a-z")));
        BOOST_TEST(test("1", ascii::char_("a-z0-9")));

        BOOST_TEST(test("x", wide::char_(L"a-z")));
        BOOST_TEST(!test("1", wide::char_(L"a-z")));
        BOOST_TEST(test("1", wide::char_(L"a-z0-9")));

        std::string set = "a-z0-9";
        BOOST_TEST(test("x", ascii::char_(set)));

#ifdef SPIRIT_NO_COMPILE_CHECK
        test("", ascii::char_(L"a-z0-9"));
#endif
    }

    return boost::report_errors();
}
