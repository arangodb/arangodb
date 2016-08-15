/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman
    Copyright (c) 2001-2010 Hartmut Kaiser

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// this file intentionally contains non-ascii characters
// boostinspect:noascii

#define BOOST_SPIRIT_X3_UNICODE

#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/type_traits/is_same.hpp>

#include <iostream>
#include "test.hpp"

int
main()
{
    using spirit_test::test;
    using spirit_test::test_attr;

    using boost::spirit::x3::unused_type;

    {
        using namespace boost::spirit::x3::ascii;
        BOOST_TEST(test("1", alnum));
        BOOST_TEST(!test(" ", alnum));
        BOOST_TEST(!test("1", alpha));
        BOOST_TEST(test("x", alpha));
        BOOST_TEST(test(" ", blank));
        BOOST_TEST(!test("x", blank));
        BOOST_TEST(test("1", digit));
        BOOST_TEST(!test("x", digit));
        BOOST_TEST(test("a", lower));
        BOOST_TEST(!test("A", lower));
        BOOST_TEST(test("!", punct));
        BOOST_TEST(!test("x", punct));
        BOOST_TEST(test(" ", space));
        BOOST_TEST(test("\n", space));
        BOOST_TEST(test("\r", space));
        BOOST_TEST(test("\t", space));
        BOOST_TEST(test("A", upper));
        BOOST_TEST(!test("a", upper));
        BOOST_TEST(test("A", xdigit));
        BOOST_TEST(test("0", xdigit));
        BOOST_TEST(test("f", xdigit));
        BOOST_TEST(!test("g", xdigit));
    }

    {
        using namespace boost::spirit::x3::ascii;
        BOOST_TEST(!test("1", ~alnum));
        BOOST_TEST(test(" ", ~alnum));
        BOOST_TEST(test("1", ~alpha));
        BOOST_TEST(!test("x", ~alpha));
        BOOST_TEST(!test(" ", ~blank));
        BOOST_TEST(test("x", ~blank));
        BOOST_TEST(!test("1", ~digit));
        BOOST_TEST(test("x", ~digit));
        BOOST_TEST(!test("a", ~lower));
        BOOST_TEST(test("A", ~lower));
        BOOST_TEST(!test("!", ~punct));
        BOOST_TEST(test("x", ~punct));
        BOOST_TEST(!test(" ", ~space));
        BOOST_TEST(!test("\n", ~space));
        BOOST_TEST(!test("\r", ~space));
        BOOST_TEST(!test("\t", ~space));
        BOOST_TEST(!test("A", ~upper));
        BOOST_TEST(test("a", ~upper));
        BOOST_TEST(!test("A", ~xdigit));
        BOOST_TEST(!test("0", ~xdigit));
        BOOST_TEST(!test("f", ~xdigit));
        BOOST_TEST(test("g", ~xdigit));
    }

    {
        using namespace boost::spirit::x3::iso8859_1;
        BOOST_TEST(test("1", alnum));
        BOOST_TEST(!test(" ", alnum));
        BOOST_TEST(!test("1", alpha));
        BOOST_TEST(test("x", alpha));
        BOOST_TEST(test(" ", blank));
        BOOST_TEST(!test("x", blank));
        BOOST_TEST(test("1", digit));
        BOOST_TEST(!test("x", digit));
        BOOST_TEST(test("a", lower));
        BOOST_TEST(!test("A", lower));
        BOOST_TEST(test("!", punct));
        BOOST_TEST(!test("x", punct));
        BOOST_TEST(test(" ", space));
        BOOST_TEST(test("\n", space));
        BOOST_TEST(test("\r", space));
        BOOST_TEST(test("\t", space));
        BOOST_TEST(test("A", upper));
        BOOST_TEST(!test("a", upper));
        BOOST_TEST(test("A", xdigit));
        BOOST_TEST(test("0", xdigit));
        BOOST_TEST(test("f", xdigit));
        BOOST_TEST(!test("g", xdigit));

// needed for VC7.1 only
#if BOOST_WORKAROUND(BOOST_MSVC, BOOST_TESTED_AT(1310))
#pragma setlocale("german")
#endif
        BOOST_TEST(test("é", alpha));
        BOOST_TEST(test("é", lower));
        BOOST_TEST(!test("é", upper));
#if BOOST_WORKAROUND(BOOST_MSVC, BOOST_TESTED_AT(1310))
#pragma setlocale("")
#endif
    }

    {
        using namespace boost::spirit::x3::standard;
        BOOST_TEST(test("1", alnum));
        BOOST_TEST(!test(" ", alnum));
        BOOST_TEST(!test("1", alpha));
        BOOST_TEST(test("x", alpha));
        BOOST_TEST(test(" ", blank));
        BOOST_TEST(!test("x", blank));
        BOOST_TEST(test("1", digit));
        BOOST_TEST(!test("x", digit));
        BOOST_TEST(test("a", lower));
        BOOST_TEST(!test("A", lower));
        BOOST_TEST(test("!", punct));
        BOOST_TEST(!test("x", punct));
        BOOST_TEST(test(" ", space));
        BOOST_TEST(test("\n", space));
        BOOST_TEST(test("\r", space));
        BOOST_TEST(test("\t", space));
        BOOST_TEST(test("A", upper));
        BOOST_TEST(!test("a", upper));
        BOOST_TEST(test("A", xdigit));
        BOOST_TEST(test("0", xdigit));
        BOOST_TEST(test("f", xdigit));
        BOOST_TEST(!test("g", xdigit));
    }

    {
        using namespace boost::spirit::x3::standard_wide;
        BOOST_TEST(test(L"1", alnum));
        BOOST_TEST(!test(L" ", alnum));
        BOOST_TEST(!test(L"1", alpha));
        BOOST_TEST(test(L"x", alpha));
        BOOST_TEST(test(L" ", blank));
        BOOST_TEST(!test(L"x", blank));
        BOOST_TEST(test(L"1", digit));
        BOOST_TEST(!test(L"x", digit));
        BOOST_TEST(test(L"a", lower));
        BOOST_TEST(!test(L"A", lower));
        BOOST_TEST(test(L"!", punct));
        BOOST_TEST(!test(L"x", punct));
        BOOST_TEST(test(L" ", space));
        BOOST_TEST(test(L"\n", space));
        BOOST_TEST(test(L"\r", space));
        BOOST_TEST(test(L"\t", space));
        BOOST_TEST(test(L"A", upper));
        BOOST_TEST(!test(L"a", upper));
        BOOST_TEST(test(L"A", xdigit));
        BOOST_TEST(test(L"0", xdigit));
        BOOST_TEST(test(L"f", xdigit));
        BOOST_TEST(!test(L"g", xdigit));
    }

    {
        using namespace boost::spirit::x3::unicode;
        BOOST_TEST(test(L"1", alnum));
        BOOST_TEST(!test(L" ", alnum));
        BOOST_TEST(!test(L"1", alpha));
        BOOST_TEST(test(L"x", alpha));
        BOOST_TEST(test(L" ", blank));
        BOOST_TEST(!test(L"x", blank));
        BOOST_TEST(test(L"1", digit));
        BOOST_TEST(!test(L"x", digit));
        BOOST_TEST(test(L"a", lower));
        BOOST_TEST(!test(L"A", lower));
        BOOST_TEST(test(L"!", punct));
        BOOST_TEST(!test(L"x", punct));
        BOOST_TEST(test(L" ", space));
        BOOST_TEST(test(L"\n", space));
        BOOST_TEST(test(L"\r", space));
        BOOST_TEST(test(L"\t", space));
        BOOST_TEST(test(L"A", upper));
        BOOST_TEST(!test(L"a", upper));
        BOOST_TEST(test(L"A", xdigit));
        BOOST_TEST(test(L"0", xdigit));
        BOOST_TEST(test(L"f", xdigit));
        BOOST_TEST(!test(L"g", xdigit));

        BOOST_TEST(test(L"A", alphabetic));
        BOOST_TEST(test(L"9", decimal_number));
        BOOST_TEST(test(L"\u2800", braille));
        BOOST_TEST(!test(L" ", braille));
        BOOST_TEST(test(L" ", ~braille));
        // $$$ TODO $$$ Add more unicode tests

// needed for VC7.1 only
#if BOOST_WORKAROUND(BOOST_MSVC, BOOST_TESTED_AT(1310))
#pragma setlocale("german")
#endif
        BOOST_TEST(test("é", alpha));
        BOOST_TEST(test("é", lower));
        BOOST_TEST(!test("é", upper));

#if BOOST_WORKAROUND(BOOST_MSVC, BOOST_TESTED_AT(1310))
#pragma setlocale("")
#endif
    }

    {   // test attribute extraction
        using boost::spirit::x3::traits::attribute_of;
        using boost::spirit::x3::iso8859_1::alpha;
        using boost::spirit::x3::iso8859_1::alpha_type;

        static_assert(
            boost::is_same<
                attribute_of<alpha_type, unused_type>::type
              , unsigned char>::value
          , "Wrong attribute type!"
        );

        int attr = 0;
        BOOST_TEST(test_attr("a", alpha, attr));
        BOOST_TEST(attr == 'a');
    }

    {   // test attribute extraction
        using boost::spirit::x3::iso8859_1::alpha;
        using boost::spirit::x3::iso8859_1::space;
        char attr = 0;
        BOOST_TEST(test_attr("     a", alpha, attr, space));
        BOOST_TEST(attr == 'a');
    }

    {   // test action
        using namespace boost::spirit::x3::ascii;
        using boost::spirit::x3::_attr;
        char ch;
        auto f = [&](auto& ctx){ ch = _attr(ctx); };

        BOOST_TEST(test("x", alnum[f]));
        BOOST_TEST(ch == 'x');
        BOOST_TEST(test("   A", alnum[f], space));
        BOOST_TEST(ch == 'A');
    }

    return boost::report_errors();
}
