/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman
    Copyright (c) 2001-2011 Hartmut Kaiser
    Copyright (c)      2019 Christian Mazakas

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#define BOOST_SPIRIT_X3_UNICODE

#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>

#include <boost/utility/string_view.hpp>

#include <iostream>
#include <vector>
#include <algorithm>

#include "test.hpp"

int
main()
{
    using spirit_test::test;

    {
        using namespace boost::spirit::x3::ascii;

        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(char_);
        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(char_('x'));
        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(char_('a', 'z'));
        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(~char_('x'));
        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(~char_('a', 'z'));
        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(~~char_('x'));
        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(~~char_('a', 'z'));

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

        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(char_);
        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(char_(L'x'));
        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(char_(L'a', L'z'));
        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(~char_(L'x'));
        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(~char_(L'a', L'z'));
        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(~~char_(L'x'));
        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(~~char_(L'a', L'z'));

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

    // unicode (normal ASCII)
    {
        using namespace boost::spirit::x3::unicode;

        BOOST_TEST(test(U"abcd", +char_(U"abcd")));
        BOOST_TEST(!test(U"abcd", +char_(U"qwer")));

        auto const sub_delims = char_(U"!$&'()*+,;=");

        auto const delims =
            std::vector<boost::u32string_view>{U"!", U"$", U"&", U"'", U"(", U")", U"*", U"+",
                                               U",", U";", U"="};

        auto const matched_all_sub_delims =
            std::all_of(delims.begin(), delims.end(), [&](auto const delim) -> bool {
                return test(delim, sub_delims);
            });

        BOOST_TEST(matched_all_sub_delims);
    }

    // unicode (escaped Unicode char literals)
    {
        using namespace boost::spirit::x3::unicode;

        auto const chars = char_(U"\u0024\u00a2\u0939\u20ac\U00010348");

        auto const test_strings =
            std::vector<boost::u32string_view>{U"\u0024", U"\u00a2", U"\u0939", U"\u20ac",
                                               U"\U00010348"};

        auto const bad_test_strings = std::vector<boost::u32string_view>{U"a", U"B", U"c", U"\u0409"};

        auto const all_matched =
            std::all_of(test_strings.begin(), test_strings.end(), [&](auto const test_str) -> bool {
                return test(test_str, chars);
            });

        auto const none_matched =
            std::all_of(bad_test_strings.begin(), bad_test_strings.end(), [&](auto const bad_test_str) -> bool {
                return !test(bad_test_str, chars);
            });

        BOOST_TEST(all_matched);
        BOOST_TEST(none_matched);
    }


    {   // single char strings!
        namespace ascii = boost::spirit::x3::ascii;
         namespace wide = boost::spirit::x3::standard_wide;

        BOOST_TEST(test("x", "x"));
        BOOST_TEST(test(L"x", L"x"));
        BOOST_TEST(test("x", ascii::char_("x")));
        BOOST_TEST(test(L"x", wide::char_(L"x")));

        BOOST_TEST(test("x", ascii::char_("a", "z")));
        BOOST_TEST(test(L"x", wide::char_(L"a", L"z")));
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

    {
        namespace ascii = boost::spirit::x3::ascii;
        char const* input = "\x80";

        // ascii > 7 bits (this should fail, not assert!)
        BOOST_TEST(!test(input, ascii::char_));
        BOOST_TEST(!test(input, ascii::char_('a')));
        BOOST_TEST(!test(input, ascii::alnum));
        BOOST_TEST(!test(input, ascii::char_("a-z")));
        BOOST_TEST(!test(input, ascii::char_('0', '9')));
    }

    return boost::report_errors();
}
