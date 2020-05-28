/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>

#include <iostream>
#include "test.hpp"

// Custom string type with a C-style string conversion.
struct custom_string_c
{
    custom_string_c(char c) { str[0] = c; str[1] = '\0'; }

    operator char*() { return str; }
    operator char const*() const { return str; }

private:
    char str[2];
};

int
main()
{
    using spirit_test::test;
    using spirit_test::test_attr;
    using boost::spirit::x3::symbols;
    using boost::spirit::x3::no_case;

    { // basics
        symbols<int> sym;

        sym.add
            ("Joel")
            ("Ruby")
            ("Tenji")
            ("Tutit")
            ("Kim")
            ("Joey")
            ("Joeyboy")
        ;

        BOOST_TEST((test("Joel", sym)));
        BOOST_TEST((test("Ruby", sym)));
        BOOST_TEST((test("Tenji", sym)));
        BOOST_TEST((test("Tutit", sym)));
        BOOST_TEST((test("Kim", sym)));
        BOOST_TEST((test("Joey", sym)));
        BOOST_TEST((test("Joeyboy", sym)));
        BOOST_TEST((!test("XXX", sym)));

        // test copy
        symbols<int> sym2;
        sym2 = sym;
        BOOST_TEST((test("Joel", sym2)));
        BOOST_TEST((test("Ruby", sym2)));
        BOOST_TEST((test("Tenji", sym2)));
        BOOST_TEST((test("Tutit", sym2)));
        BOOST_TEST((test("Kim", sym2)));
        BOOST_TEST((test("Joey", sym2)));
        BOOST_TEST((!test("XXX", sym2)));

        // make sure it plays well with other parsers
        BOOST_TEST((test("Joelyo", sym >> "yo")));

        sym.remove
            ("Joel")
            ("Ruby")
        ;

        BOOST_TEST((!test("Joel", sym)));
        BOOST_TEST((!test("Ruby", sym)));
    }

    { // comma syntax
        symbols<int> sym;
        sym += "Joel", "Ruby", "Tenji", "Tutit", "Kim", "Joey";

        BOOST_TEST((test("Joel", sym)));
        BOOST_TEST((test("Ruby", sym)));
        BOOST_TEST((test("Tenji", sym)));
        BOOST_TEST((test("Tutit", sym)));
        BOOST_TEST((test("Kim", sym)));
        BOOST_TEST((test("Joey", sym)));
        BOOST_TEST((!test("XXX", sym)));

        sym -= "Joel", "Ruby";

        BOOST_TEST((!test("Joel", sym)));
        BOOST_TEST((!test("Ruby", sym)));
    }

    { // no-case handling
        using namespace boost::spirit::x3::ascii;

        symbols<int> sym;
        // NOTE: make sure all entries are in lower-case!!!
        sym = "joel", "ruby", "tenji", "tutit", "kim", "joey";

        BOOST_TEST((test("joel", no_case[sym])));
        BOOST_TEST((test("ruby", no_case[sym])));
        BOOST_TEST((test("tenji", no_case[sym])));
        BOOST_TEST((test("tutit", no_case[sym])));
        BOOST_TEST((test("kim", no_case[sym])));
        BOOST_TEST((test("joey", no_case[sym])));

        BOOST_TEST((test("JOEL", no_case[sym])));
        BOOST_TEST((test("RUBY", no_case[sym])));
        BOOST_TEST((test("TENJI", no_case[sym])));
        BOOST_TEST((test("TUTIT", no_case[sym])));
        BOOST_TEST((test("KIM", no_case[sym])));
        BOOST_TEST((test("JOEY", no_case[sym])));

        // make sure it plays well with other parsers
        BOOST_TEST((test("Joelyo", no_case[sym] >> "yo")));
    }

    { // attributes
        symbols<int> sym;

        sym.add
            ("Joel", 1)
            ("Ruby", 2)
            ("Tenji", 3)
            ("Tutit", 4)
            ("Kim", 5)
            ("Joey", 6)
        ;

        int i;
        BOOST_TEST((test_attr("Joel", sym, i)));
        BOOST_TEST(i == 1);
        BOOST_TEST((test_attr("Ruby", sym, i)));
        BOOST_TEST(i == 2);
        BOOST_TEST((test_attr("Tenji", sym, i)));
        BOOST_TEST(i == 3);
        BOOST_TEST((test_attr("Tutit", sym, i)));
        BOOST_TEST(i == 4);
        BOOST_TEST((test_attr("Kim", sym, i)));
        BOOST_TEST(i == 5);
        BOOST_TEST((test_attr("Joey", sym, i)));
        BOOST_TEST(i == 6);
        BOOST_TEST((!test_attr("XXX", sym, i)));

        // double add:

        sym.add("Joel", 265);
        BOOST_TEST((test_attr("Joel", sym, i)));
        BOOST_TEST(i == 1);
    }

    { // actions
        using boost::spirit::x3::_attr;

        symbols<int> sym;
        sym.add
            ("Joel", 1)
            ("Ruby", 2)
            ("Tenji", 3)
            ("Tutit", 4)
            ("Kim", 5)
            ("Joey", 6)
        ;

        int i;
        auto f = [&](auto& ctx){ i = _attr(ctx); };

        BOOST_TEST((test("Joel", sym[f])));
        BOOST_TEST(i == 1);
        BOOST_TEST((test("Ruby", sym[f])));
        BOOST_TEST(i == 2);
        BOOST_TEST((test("Tenji", sym[f])));
        BOOST_TEST(i == 3);
        BOOST_TEST((test("Tutit", sym[f])));
        BOOST_TEST(i == 4);
        BOOST_TEST((test("Kim", sym[f])));
        BOOST_TEST(i == 5);
        BOOST_TEST((test("Joey", sym[f])));
        BOOST_TEST(i == 6);
        BOOST_TEST((!test("XXX", sym[f])));
    }

    return boost::report_errors();
}
