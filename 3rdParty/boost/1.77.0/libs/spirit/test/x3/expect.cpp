/*=============================================================================
    Copyright (c) 2001-2013 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/vector.hpp>
#include <boost/fusion/include/at.hpp>

#include <string>
#include <iostream>
#include "test.hpp"

int
main()
{
    using namespace boost::spirit;
    using namespace boost::spirit::x3::ascii;
    using boost::spirit::x3::lit;
    using boost::spirit::x3::expect;
    using spirit_test::test;
    using spirit_test::test_attr;
    using boost::spirit::x3::expectation_failure;

    BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(expect['x']);
    BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(char_ > char_);

    {
        try
        {
            BOOST_TEST((test("aa", char_ >> expect[char_])));
            BOOST_TEST((test("aaa", char_ >> expect[char_ >> char_('a')])));
            BOOST_TEST((test("xi", char_('x') >> expect[char_('i')])));
            BOOST_TEST((!test("xi", char_('y') >> expect[char_('o')]))); // should not throw!
            BOOST_TEST((test("xin", char_('x') >> expect[char_('i') >> char_('n')])));
            BOOST_TEST((!test("xi", char_('x') >> expect[char_('o')])));
        }
        catch (expectation_failure<char const*> const& x)
        {
            std::cout << "expected: " << x.which();
            std::cout << " got: \"" << x.where() << '"' << std::endl;
        }
    }

    {
        try
        {
            BOOST_TEST((test("aa", char_ > char_)));
            BOOST_TEST((test("aaa", char_ > char_ > char_('a'))));
            BOOST_TEST((test("xi", char_('x') > char_('i'))));
            BOOST_TEST((!test("xi", char_('y') > char_('o')))); // should not throw!
            BOOST_TEST((test("xin", char_('x') > char_('i') > char_('n'))));
            BOOST_TEST((!test("xi", char_('x') > char_('o'))));
        }
        catch (expectation_failure<char const*> const& x)
        {
            std::cout << "expected: " << x.which();
            std::cout << " got: \"" << x.where() << '"' << std::endl;
        }
    }

    {
        try
        {
            BOOST_TEST((!test("ay:a", char_ > char_('x') >> ':' > 'a')));
        }
        catch (expectation_failure<char const*> const& x)
        {
            std::cout << "expected: " << x.which();
            std::cout << " got: \"" << x.where() << '"' << std::endl;
        }
    }

#if defined(BOOST_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-shift-op-parentheses"
#endif
    { // Test that attributes with > (sequences) work just like >> (sequences)

        using boost::fusion::vector;
        using boost::fusion::at_c;

        {
            vector<char, char, char> attr;
            BOOST_TEST((test_attr(" a\n  b\n  c",
                char_ > char_ > char_, attr, space)));
            BOOST_TEST((at_c<0>(attr) == 'a'));
            BOOST_TEST((at_c<1>(attr) == 'b'));
            BOOST_TEST((at_c<2>(attr) == 'c'));
        }

        {
            vector<char, char, char> attr;
            BOOST_TEST((test_attr(" a\n  b\n  c",
                char_ > char_ >> char_, attr, space)));
            BOOST_TEST((at_c<0>(attr) == 'a'));
            BOOST_TEST((at_c<1>(attr) == 'b'));
            BOOST_TEST((at_c<2>(attr) == 'c'));
        }

        {
            vector<char, char, char> attr;
            BOOST_TEST((test_attr(" a, b, c",
                char_ >> ',' > char_ >> ',' > char_, attr, space)));
            BOOST_TEST((at_c<0>(attr) == 'a'));
            BOOST_TEST((at_c<1>(attr) == 'b'));
            BOOST_TEST((at_c<2>(attr) == 'c'));
        }

        {
            std::string attr;
            BOOST_TEST((test_attr("'azaaz'",
                "'" > *(char_("a") | char_("z")) > "'", attr, space)));
            BOOST_TEST(attr == "azaaz");
        }
    }
#if defined(BOOST_CLANG)
#pragma clang diagnostic pop
#endif

    {
        try
        {
            BOOST_TEST((test(" a a", char_ > char_, space)));
            BOOST_TEST((test(" x i", char_('x') > char_('i'), space)));
            BOOST_TEST((!test(" x i", char_('x') > char_('o'), space)));
        }
        catch (expectation_failure<char const*> const& x)
        {
            std::cout << "expected: " << x.which();
            std::cout << " got: \"" << x.where() << '"' << std::endl;
        }
    }

    {
        try
        {
            BOOST_TEST((test("bar", expect[lit("foo")])));
        }
        catch (expectation_failure<char const*> const& x)
        {
            std::cout << "expected: " << x.which();
            std::cout << " got: \"" << x.where() << '"' << std::endl;
        }
    }

    return boost::report_errors();
}
