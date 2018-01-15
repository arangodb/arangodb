// ------------------------------------------------------------------------------
// format_test2.cpp :  a few real, simple tests.
// ------------------------------------------------------------------------------

//  Copyright Samuel Krempp 2003. Use, modification, and distribution are
//  subject to the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// see http://www.boost.org/libs/format for library home page

// ------------------------------------------------------------------------------

#include "boost/format.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/config.hpp>
#include <boost/predef.h>

#include <iostream>
#include <iomanip>

#if !defined(BOOST_NO_STD_LOCALE)
#include <locale>
#endif

#define BOOST_INCLUDE_MAIN
#include <boost/test/test_tools.hpp>


struct Rational {
  int n,d;
  Rational (int an, int ad) : n(an), d(ad) {}
};

std::ostream& operator<<( std::ostream& os, const Rational& r) {
  os << r.n << "/" << r.d;
  return os;
}

#if !defined(BOOST_NO_STD_LOCALE)
// in C++03 this has to be globally defined or gcc complains
struct custom_tf : std::numpunct<char> {
    std::string do_truename()  const { return "POSITIVE"; }
    std::string do_falsename() const { return "NEGATIVE"; }
};
#endif

int test_main(int, char* [])
{
    using namespace std;
    using boost::format;
    using boost::io::group;
    using boost::str;

    Rational r(16,9);
    const Rational cr(9,16);

    string s;
    s = str(format("%5%. %5$=6s . %1% format %5%, c'%3% %1% %2%.\n")
            % "le" % "bonheur" % "est" % "trop" % group(setfill('_'), "bref") );

    if(s  != "bref. _bref_ . le format bref, c'est le bonheur.\n") {
      cerr << s;
      BOOST_ERROR("centered alignement : formatting result incorrect");
    }


    s = str(format("%+8d %-8d\n") % r % cr );
    if(s  != "  +16/+9 9/16    \n") {
      cerr << s;
      BOOST_ERROR("(user-type) formatting result incorrect");
    }

    s = str(format("[%0+4d %0+8d %-08d]\n") % 8 % r % r);
    if(s  != "[+008 +0016/+9 16/9    ]\n") {
      cerr << s;
      BOOST_ERROR("(zero-padded user-type) formatting result incorrect");
    }


    s = str( format("%1%, %20T_ (%|2$5|,%|3$5|)\n") % "98765" % 1326 % 88 ) ;
    if( s != "98765, _____________ ( 1326,   88)\n" )
            BOOST_ERROR("(tabulation) formatting result incorrect");
    s = str( format("%s, %|20t|=") % 88 ) ;
    if( s != "88,                 =" ) {
      cout << s << endl;
      BOOST_ERROR("(tabulation) formatting result incorrect");
    }


    s = str(format("%.2s %8c.\n") % "root" % "user" );
    if(s  != "ro        u.\n") {
      cerr << s;
      BOOST_ERROR("(truncation) formatting result incorrect");
    }

   // width in format-string is overridden by setw manipulator :
    s = str( format("%|1$4| %|1$|") % group(setfill('0'), setw(6), 1) );
    if( s!= "000001 000001")
      BOOST_ERROR("width in format VS in argument misbehaved");

    s = str( format("%|=s|") % group(setfill('_'), setw(6), r) );
    if( s!= "_16/9_") {
      cerr << s << endl;
      BOOST_ERROR("width in group context is not handled correctly");
    }


    // options that uses internal alignment : + 0 #
    s = str( format("%+6d %0#6x %s\n")  % 342 % 33 % "ok" );
    if( s !="  +342 0x0021 ok\n")
      BOOST_ERROR("(flags +, 0, or #) formatting result incorrect");

    // flags in the format string are not sticky
    // and hex in argument overrrides type-char d (->decimal) :
    s = str( format("%2$#4d %|1$4| %|2$#4| %|3$|")
             % 101
             % group(setfill('_'), hex, 2)
             % 103 );
    if(s != "_0x2  101 _0x2 103")
      BOOST_ERROR("formatting error. (not-restoring state ?)");



    // flag '0' is tricky .
    // left-align cancels '0':
    s = str( format("%2$0#12X %2$0#-12d %1$0#10d \n") % -20 % 10 );
    if( s != "0X000000000A 10           -000000020 \n"){
      cerr << s;
      BOOST_ERROR("formatting error. (flag 0)");
    }

    // actually testing floating point output is implementation
    // specific so we're just going to do minimal checking...
    double dbl = 1234567.890123f;

#if (__cplusplus >= 201103L) || (BOOST_VERSION_NUMBER_MAJOR(BOOST_COMP_MSVC) >= 12)
    // msvc-12.0 and later have support for hexfloat but do not set __cplusplus to a C++11 value
    BOOST_CHECK(boost::starts_with((boost::format("%A") % dbl).str(), "0X"));
    BOOST_CHECK(boost::starts_with((boost::format("%a") % dbl).str(), "0x"));
#endif

    BOOST_CHECK(boost::contains((boost::format("%E") % dbl).str(), "E"));
    BOOST_CHECK(boost::contains((boost::format("%e") % dbl).str(), "e"));
    BOOST_CHECK(boost::contains((boost::format("%F") % dbl).str(), "."));
    BOOST_CHECK(boost::contains((boost::format("%f") % dbl).str(), "."));
    BOOST_CHECK(!(boost::format("%G") % dbl).str().empty());
    BOOST_CHECK(!(boost::format("%g") % dbl).str().empty());

    // testing argument type parsing - remember argument types are ignored
    // because operator % presents the argument type.
    unsigned int value = 456;
    BOOST_CHECK_EQUAL((boost::format("%hhu") % value).str(), "456");
    BOOST_CHECK_EQUAL((boost::format("%hu") % value).str(), "456");
    BOOST_CHECK_EQUAL((boost::format("%lu") % value).str(), "456");
    BOOST_CHECK_EQUAL((boost::format("%llu") % value).str(), "456");
    BOOST_CHECK_EQUAL((boost::format("%ju") % value).str(), "456");
    BOOST_CHECK_EQUAL((boost::format("%zu") % value).str(), "456");
    BOOST_CHECK(boost::starts_with((boost::format("%Lf") % value).str(), "456"));

#if !defined(BOOST_NO_STD_LOCALE)
    // boolalpha support
    std::locale loc;
    const std::numpunct<char>& punk(std::use_facet<std::numpunct<char> >(loc));

    // Demonstrates how to modify the default string to something else
    std::locale custom(std::locale(), new custom_tf);
    boost::ignore_unused(locale::global(custom));
    BOOST_CHECK_EQUAL((boost::format("%b") % false).str(), "NEGATIVE");
    BOOST_CHECK_EQUAL((boost::format("%b") % true).str(), "POSITIVE");

    // restore system default
    locale::global(loc);
    BOOST_CHECK_EQUAL((boost::format("%b") % false).str(), punk.falsename());
    BOOST_CHECK_EQUAL((boost::format("%b") % true).str(), punk.truename());
#endif

    // Support for microsoft argument type specifiers: 'w' (same as 'l'), I, I32, I64
    BOOST_CHECK_EQUAL((boost::format("%wc") % '5').str(), "5");
    BOOST_CHECK_EQUAL((boost::format("%Id") % 123).str(), "123");
    BOOST_CHECK_EQUAL((boost::format("%I32d") % 456).str(), "456");
    BOOST_CHECK_EQUAL((boost::format("%I64d") % 789).str(), "789");

    // issue-36 volatile (and const) keyword
    volatile int vint = 1234567;
    BOOST_CHECK_EQUAL((boost::format("%1%") % vint).str(), "1234567");
    volatile const int vcint = 7654321;
    BOOST_CHECK_EQUAL((boost::format("%1%") % vcint).str(), "7654321");

    // skip width if '*'
    BOOST_CHECK_EQUAL((boost::format("%*d") % vint).str(), "1234567");

    // internal ios flag
    BOOST_CHECK_EQUAL((boost::format("%_6d") % -77).str(), "-   77");

    // combining some flags
    BOOST_CHECK_EQUAL((boost::format("%+05.5d"  ) %  77).str(), "+0077");
    BOOST_CHECK_EQUAL((boost::format("%+ 5.5d"  ) %  77).str(), "  +77");
    BOOST_CHECK_EQUAL((boost::format("%+_ 5.5d" ) %  77).str(), "+  77");
    BOOST_CHECK_EQUAL((boost::format("%+- 5.5d" ) %  77).str(), "+77  ");
    BOOST_CHECK_EQUAL((boost::format("%+05.5d"  ) % -77).str(), "-0077");
    BOOST_CHECK_EQUAL((boost::format("%+ 5.5d"  ) % -77).str(), "  -77");
    BOOST_CHECK_EQUAL((boost::format("%+_ 5.5d" ) % -77).str(), "-  77");
    BOOST_CHECK_EQUAL((boost::format("%+- 5.5d" ) % -77).str(), "-77  ");

    // reuse state and reset format flags
    std::string mystr("abcdefghijklmnop");
    BOOST_CHECK_EQUAL((boost::format("%2.2s %-4.4s % 8.8s")
        % mystr % mystr % mystr).str(), "ab abcd  abcdefg");

    // coverage
    format fmt("%1%%2%%3%");
    fmt = fmt;

    return 0;
}
