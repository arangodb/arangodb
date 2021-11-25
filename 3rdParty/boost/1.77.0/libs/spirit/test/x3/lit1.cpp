/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman
    http://spirit.sourceforge.net/

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/vector.hpp>

#include <string>
#include "test.hpp"

int
main()
{
    using spirit_test::test;
    using spirit_test::test_attr;
    using boost::spirit::x3::string;

    BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(string("x"));

    {
        BOOST_TEST((test("kimpo", "kimpo")));
        BOOST_TEST((test("kimpo", string("kimpo"))));

        BOOST_TEST((test("x", string("x"))));
        BOOST_TEST((test(L"x", string(L"x"))));

        std::basic_string<char> s("kimpo");
        std::basic_string<wchar_t> ws(L"kimpo");
        BOOST_TEST((test("kimpo", s)));
        BOOST_TEST((test(L"kimpo", ws)));
        BOOST_TEST((test("kimpo", string(s))));
        BOOST_TEST((test(L"kimpo", string(ws))));
    }

    {
        BOOST_TEST((test(L"kimpo", L"kimpo")));
        BOOST_TEST((test(L"kimpo", string(L"kimpo"))));
        BOOST_TEST((test(L"x", string(L"x"))));
    }

    {
        std::basic_string<char> s("kimpo");
        BOOST_TEST((test("kimpo", string(s))));

        std::basic_string<wchar_t> ws(L"kimpo");
        BOOST_TEST((test(L"kimpo", string(ws))));
    }

    {
        using namespace boost::spirit::x3::ascii;
        BOOST_TEST((test("    kimpo", string("kimpo"), space)));
        BOOST_TEST((test(L"    kimpo", string(L"kimpo"), space)));
        BOOST_TEST((test("    x", string("x"), space)));
    }

    {
        using namespace boost::spirit::x3::ascii;
        BOOST_TEST((test("    kimpo", string("kimpo"), space)));
        BOOST_TEST((test(L"    kimpo", string(L"kimpo"), space)));
        BOOST_TEST((test("    x", string("x"), space)));
    }

    {
        using namespace boost::spirit::x3::ascii;
        std::string s;
        BOOST_TEST((test_attr("kimpo", string("kimpo"), s)));
        BOOST_TEST(s == "kimpo");
        s.clear();
        BOOST_TEST((test_attr("kimpo", string("kim") >> string("po"), s)));
        BOOST_TEST(s == "kimpo");
        s.clear();
        BOOST_TEST((test_attr(L"kimpo", string(L"kimpo"), s)));
        BOOST_TEST(s == "kimpo");
        s.clear();
        BOOST_TEST((test_attr("x", string("x"), s)));
        BOOST_TEST(s == "x");
    }

    { // single-element fusion vector tests
        boost::fusion::vector<std::string> s;
        BOOST_TEST(test_attr("kimpo", string("kimpo"), s));
        BOOST_TEST(boost::fusion::at_c<0>(s) == "kimpo");
    }

    return boost::report_errors();
}
