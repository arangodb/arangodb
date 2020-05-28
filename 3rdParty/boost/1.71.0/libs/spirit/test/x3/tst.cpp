/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3/string/tst.hpp>
#include <boost/spirit/home/x3/string/tst_map.hpp>
#include <boost/spirit/home/x3/support/no_case.hpp>
#include <boost/spirit/home/support/char_encoding/standard.hpp>
#include <boost/spirit/home/support/char_encoding/standard_wide.hpp>
#include <string>
#include <cctype>
#include <iostream>

namespace
{
    template <typename TST, typename Char>
    void add(TST& tst, Char const* s, int data)
    {
        Char const* last = s;
        while (*last)
            last++;
        tst.add(s, last, data);
    }

    template <typename TST, typename Char>
    void remove(TST& tst, Char const* s)
    {
        Char const* last = s;
        while (*last)
            last++;
        tst.remove(s, last);
    }

    template <typename TST, typename Char, typename CaseCompare>
    void docheck(TST const& tst, CaseCompare const& comp, Char const* s, bool expected, int N = 0, int val = -1)
    {
        Char const* first = s;
        Char const* last = s;
        while (*last)
            last++;
        int* r = tst.find(s, last,comp);
        BOOST_TEST((r != 0) == expected);
        if (r != 0)
            BOOST_TEST((s-first) == N);
        if (r)
            BOOST_TEST(*r == val);
    }

    struct printer
    {
        template <typename String, typename Data>
        void operator()(String const& s, Data const& data)
        {
            std::cout << "    " << s << ": " << data << std::endl;
        }
    };

    template <typename TST>
    void print(TST const& tst)
    {
        std::cout << '[' << std::endl;
        tst.for_each(printer());
        std::cout << ']' << std::endl;
    }

}

boost::spirit::x3::case_compare<boost::spirit::char_encoding::standard> ncomp;
boost::spirit::x3::case_compare<boost::spirit::char_encoding::standard_wide> wcomp;
boost::spirit::x3::no_case_compare<boost::spirit::char_encoding::standard> nc_ncomp;
boost::spirit::x3::no_case_compare<boost::spirit::char_encoding::standard_wide> nc_wcomp;

template <typename Lookup, typename WideLookup>
void tests()
{
    { // basic tests
        Lookup lookup;

        docheck(lookup, ncomp, "not-yet-there", false);
        docheck(lookup, ncomp, "", false);

        add(lookup, "apple", 123);
        docheck(lookup, ncomp, "apple", true, 5, 123); // full match
        docheck(lookup, ncomp, "banana", false); // no-match
        docheck(lookup, ncomp, "applexxx", true, 5, 123); // partial match

        add(lookup, "applepie", 456);
        docheck(lookup, ncomp, "applepie", true, 8, 456); // full match
        docheck(lookup, ncomp, "banana", false); // no-match
        docheck(lookup, ncomp, "applepiexxx", true, 8, 456); // partial match
        docheck(lookup, ncomp, "apple", true, 5, 123); // full match
        docheck(lookup, ncomp, "applexxx", true, 5, 123); // partial match
    }

    { // variation of above
        Lookup lookup;

        add(lookup, "applepie", 456);
        add(lookup, "apple", 123);

        docheck(lookup, ncomp, "applepie", true, 8, 456); // full match
        docheck(lookup, ncomp, "banana", false); // no-match
        docheck(lookup, ncomp, "applepiexxx", true, 8, 456); // partial match
        docheck(lookup, ncomp, "apple", true, 5, 123); // full match
        docheck(lookup, ncomp, "applexxx", true, 5, 123); // partial match
    }
    { // variation of above
        Lookup lookup;

        add(lookup, "applepie", 456);
        add(lookup, "apple", 123);

        docheck(lookup, ncomp, "applepie", true, 8, 456); // full match
        docheck(lookup, ncomp, "banana", false); // no-match
        docheck(lookup, ncomp, "applepiexxx", true, 8, 456); // partial match
        docheck(lookup, ncomp, "apple", true, 5, 123); // full match
        docheck(lookup, ncomp, "applexxx", true, 5, 123); // partial match
    }

    { // narrow char tests
        Lookup lookup;
        add(lookup, "pineapple", 1);
        add(lookup, "orange", 2);
        add(lookup, "banana", 3);
        add(lookup, "applepie", 4);
        add(lookup, "apple", 5);

        docheck(lookup, ncomp, "pineapple", true, 9, 1);
        docheck(lookup, ncomp, "orange", true, 6, 2);
        docheck(lookup, ncomp, "banana", true, 6, 3);
        docheck(lookup, ncomp, "apple", true, 5, 5);
        docheck(lookup, ncomp, "pizza", false);
        docheck(lookup, ncomp, "steak", false);
        docheck(lookup, ncomp, "applepie", true, 8, 4);
        docheck(lookup, ncomp, "bananarama", true, 6, 3);
        docheck(lookup, ncomp, "applet", true, 5, 5);
        docheck(lookup, ncomp, "applepi", true, 5, 5);
        docheck(lookup, ncomp, "appl", false);

        docheck(lookup, ncomp, "pineapplez", true, 9, 1);
        docheck(lookup, ncomp, "orangez", true, 6, 2);
        docheck(lookup, ncomp, "bananaz", true, 6, 3);
        docheck(lookup, ncomp, "applez", true, 5, 5);
        docheck(lookup, ncomp, "pizzaz", false);
        docheck(lookup, ncomp, "steakz", false);
        docheck(lookup, ncomp, "applepiez", true, 8, 4);
        docheck(lookup, ncomp, "bananaramaz", true, 6, 3);
        docheck(lookup, ncomp, "appletz", true, 5, 5);
        docheck(lookup, ncomp, "applepix", true, 5, 5);
    }

    { // wide char tests
        WideLookup lookup;
        add(lookup, L"pineapple", 1);
        add(lookup, L"orange", 2);
        add(lookup, L"banana", 3);
        add(lookup, L"applepie", 4);
        add(lookup, L"apple", 5);

        docheck(lookup, wcomp, L"pineapple", true, 9, 1);
        docheck(lookup, wcomp, L"orange", true, 6, 2);
        docheck(lookup, wcomp, L"banana", true, 6, 3);
        docheck(lookup, wcomp, L"apple", true, 5, 5);
        docheck(lookup, wcomp, L"pizza", false);
        docheck(lookup, wcomp, L"steak", false);
        docheck(lookup, wcomp, L"applepie", true, 8, 4);
        docheck(lookup, wcomp, L"bananarama", true, 6, 3);
        docheck(lookup, wcomp, L"applet", true, 5, 5);
        docheck(lookup, wcomp, L"applepi", true, 5, 5);
        docheck(lookup, wcomp, L"appl", false);

        docheck(lookup, wcomp, L"pineapplez", true, 9, 1);
        docheck(lookup, wcomp, L"orangez", true, 6, 2);
        docheck(lookup, wcomp, L"bananaz", true, 6, 3);
        docheck(lookup, wcomp, L"applez", true, 5, 5);
        docheck(lookup, wcomp, L"pizzaz", false);
        docheck(lookup, wcomp, L"steakz", false);
        docheck(lookup, wcomp, L"applepiez", true, 8, 4);
        docheck(lookup, wcomp, L"bananaramaz", true, 6, 3);
        docheck(lookup, wcomp, L"appletz", true, 5, 5);
        docheck(lookup, wcomp, L"applepix", true, 5, 5);
    }

    { // test remove
        Lookup lookup;
        add(lookup, "pineapple", 1);
        add(lookup, "orange", 2);
        add(lookup, "banana", 3);
        add(lookup, "applepie", 4);
        add(lookup, "apple", 5);

        docheck(lookup, ncomp, "pineapple", true, 9, 1);
        docheck(lookup, ncomp, "orange", true, 6, 2);
        docheck(lookup, ncomp, "banana", true, 6, 3);
        docheck(lookup, ncomp, "apple", true, 5, 5);
        docheck(lookup, ncomp, "applepie", true, 8, 4);
        docheck(lookup, ncomp, "bananarama", true, 6, 3);
        docheck(lookup, ncomp, "applet", true, 5, 5);
        docheck(lookup, ncomp, "applepi", true, 5, 5);
        docheck(lookup, ncomp, "appl", false);

        remove(lookup, "banana");
        docheck(lookup, ncomp, "pineapple", true, 9, 1);
        docheck(lookup, ncomp, "orange", true, 6, 2);
        docheck(lookup, ncomp, "banana", false);
        docheck(lookup, ncomp, "apple", true, 5, 5);
        docheck(lookup, ncomp, "applepie", true, 8, 4);
        docheck(lookup, ncomp, "bananarama", false);
        docheck(lookup, ncomp, "applet", true, 5, 5);
        docheck(lookup, ncomp, "applepi", true, 5, 5);
        docheck(lookup, ncomp, "appl", false);

        remove(lookup, "apple");
        docheck(lookup, ncomp, "pineapple", true, 9, 1);
        docheck(lookup, ncomp, "orange", true, 6, 2);
        docheck(lookup, ncomp, "apple", false);
        docheck(lookup, ncomp, "applepie", true, 8, 4);
        docheck(lookup, ncomp, "applet", false);
        docheck(lookup, ncomp, "applepi", false);
        docheck(lookup, ncomp, "appl", false);

        remove(lookup, "orange");
        docheck(lookup, ncomp, "pineapple", true, 9, 1);
        docheck(lookup, ncomp, "orange", false);
        docheck(lookup, ncomp, "applepie", true, 8, 4);

        remove(lookup, "pineapple");
        docheck(lookup, ncomp, "pineapple", false);
        docheck(lookup, ncomp, "orange", false);
        docheck(lookup, ncomp, "applepie", true, 8, 4);

        remove(lookup, "applepie");
        docheck(lookup, ncomp, "applepie", false);
    }

    { // copy/assign/clear test
        Lookup lookupa;
        add(lookupa, "pineapple", 1);
        add(lookupa, "orange", 2);
        add(lookupa, "banana", 3);
        add(lookupa, "applepie", 4);
        add(lookupa, "apple", 5);

        Lookup lookupb(lookupa); // copy ctor
        docheck(lookupb, ncomp, "pineapple", true, 9, 1);
        docheck(lookupb, ncomp, "orange", true, 6, 2);
        docheck(lookupb, ncomp, "banana", true, 6, 3);
        docheck(lookupb, ncomp, "apple", true, 5, 5);
        docheck(lookupb, ncomp, "pizza", false);
        docheck(lookupb, ncomp, "steak", false);
        docheck(lookupb, ncomp, "applepie", true, 8, 4);
        docheck(lookupb, ncomp, "bananarama", true, 6, 3);
        docheck(lookupb, ncomp, "applet", true, 5, 5);
        docheck(lookupb, ncomp, "applepi", true, 5, 5);
        docheck(lookupb, ncomp, "appl", false);

        lookupb.clear(); // clear
        docheck(lookupb, ncomp, "pineapple", false);
        docheck(lookupb, ncomp, "orange", false);
        docheck(lookupb, ncomp, "banana", false);
        docheck(lookupb, ncomp, "apple", false);
        docheck(lookupb, ncomp, "applepie", false);
        docheck(lookupb, ncomp, "bananarama", false);
        docheck(lookupb, ncomp, "applet", false);
        docheck(lookupb, ncomp, "applepi", false);
        docheck(lookupb, ncomp, "appl", false);

        lookupb = lookupa; // assign
        docheck(lookupb, ncomp, "pineapple", true, 9, 1);
        docheck(lookupb, ncomp, "orange", true, 6, 2);
        docheck(lookupb, ncomp, "banana", true, 6, 3);
        docheck(lookupb, ncomp, "apple", true, 5, 5);
        docheck(lookupb, ncomp, "pizza", false);
        docheck(lookupb, ncomp, "steak", false);
        docheck(lookupb, ncomp, "applepie", true, 8, 4);
        docheck(lookupb, ncomp, "bananarama", true, 6, 3);
        docheck(lookupb, ncomp, "applet", true, 5, 5);
        docheck(lookupb, ncomp, "applepi", true, 5, 5);
        docheck(lookupb, ncomp, "appl", false);
    }

    { // test for_each
        Lookup lookup;
        add(lookup, "pineapple", 1);
        add(lookup, "orange", 2);
        add(lookup, "banana", 3);
        add(lookup, "applepie", 4);
        add(lookup, "apple", 5);

        print(lookup);
    }

    { // case insensitive tests
        Lookup lookup;

        // NOTE: make sure all entries are in lower-case!!!
        add(lookup, "pineapple", 1);
        add(lookup, "orange", 2);
        add(lookup, "banana", 3);
        add(lookup, "applepie", 4);
        add(lookup, "apple", 5);

        docheck(lookup, nc_ncomp, "pineapple", true, 9, 1);
        docheck(lookup, nc_ncomp, "orange", true, 6, 2);
        docheck(lookup, nc_ncomp, "banana", true, 6, 3);
        docheck(lookup, nc_ncomp, "apple", true, 5, 5);
        docheck(lookup, nc_ncomp, "applepie", true, 8, 4);

        docheck(lookup, nc_ncomp, "PINEAPPLE", true, 9, 1);
        docheck(lookup, nc_ncomp, "ORANGE", true, 6, 2);
        docheck(lookup, nc_ncomp, "BANANA", true, 6, 3);
        docheck(lookup, nc_ncomp, "APPLE", true, 5, 5);
        docheck(lookup, nc_ncomp, "APPLEPIE", true, 8, 4);

        docheck(lookup, nc_ncomp, "pineApple", true, 9, 1);
        docheck(lookup, nc_ncomp, "orangE", true, 6, 2);
        docheck(lookup, nc_ncomp, "Banana", true, 6, 3);
        docheck(lookup, nc_ncomp, "aPPLe", true, 5, 5);
        docheck(lookup, nc_ncomp, "ApplePie", true, 8, 4);

        print(lookup);
    }
}

int main()
{
    using boost::spirit::x3::tst;
    using boost::spirit::x3::tst_map;

    tests<tst<char, int>, tst<wchar_t, int> >();
//~    tests<tst_map<char, int>, tst_map<wchar_t, int> >();

    return boost::report_errors();
}
