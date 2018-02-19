// ------------------------------------------------------------------------------
// format_test_exceptions.cpp : exception handling
// ------------------------------------------------------------------------------

//  Copyright 2017 James E. King, III - Use, modification, and distribution are
//  subject to the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// see http://www.boost.org/libs/format for library home page

// ------------------------------------------------------------------------------

#define BOOST_FORMAT_STATIC_STREAM
#include "boost/format.hpp"

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <iomanip>

#define BOOST_INCLUDE_MAIN
#include <boost/test/test_tools.hpp>

#define CHECK_INVALID_0(FMT, EX) { BOOST_CHECK_THROW((boost::format(FMT)).str(), EX); \
    boost::format safe; safe.exceptions(boost::io::no_error_bits); BOOST_CHECK_NO_THROW(safe.parse(FMT)); BOOST_CHECK_NO_THROW((safe).str()); }
#define CHECK_INVALID_1(FMT, _1, EX) { BOOST_CHECK_THROW((boost::format(FMT) % _1).str(), EX); \
    boost::format safe; safe.exceptions(boost::io::no_error_bits); BOOST_CHECK_NO_THROW(safe.parse(FMT)); BOOST_CHECK_NO_THROW((safe % _1).str()); }
#define CHECK_INVALID_2(FMT, _1, _2, EX) { BOOST_CHECK_THROW((boost::format(FMT) % _1 % _2).str(), EX); \
    boost::format safe; safe.exceptions(boost::io::no_error_bits); BOOST_CHECK_NO_THROW(safe.parse(FMT)); BOOST_CHECK_NO_THROW((safe % _1 % _2).str()); }

int test_main(int, char* [])
{
    using namespace boost::io;

    // https://svn.boost.org/trac10/ticket/8735

    CHECK_INVALID_0("%", bad_format_string);          // no conversion specifier
    CHECK_INVALID_0("%|", bad_format_string);         // truncated
    CHECK_INVALID_0("%|%", bad_format_string);        // mismatched bars
    CHECK_INVALID_0("%|2%", bad_format_string);       // mismatched bars
    CHECK_INVALID_0("%'", bad_format_string);         // flag ' is ignored, no conversion specifier
    CHECK_INVALID_0("%2", bad_format_string);         // no terminating percent
    CHECK_INVALID_0("%*", bad_format_string);         // truncated
    CHECK_INVALID_1("%$2", 1, bad_format_string);     // no conversion specifier
    CHECK_INVALID_1("%$2.*", 1, bad_format_string);   // no conversion specifier
    CHECK_INVALID_0("%0%", bad_format_string);        // positional arguments start at 1
    CHECK_INVALID_2("%1%", 1, 2, too_many_args);
    CHECK_INVALID_1("%1% %2%", 1, too_few_args);

    CHECK_INVALID_0("%I", bad_format_string);
    CHECK_INVALID_0("%I2", bad_format_string);
    CHECK_INVALID_0("%I2d", bad_format_string);
    CHECK_INVALID_0("%I3", bad_format_string);
    CHECK_INVALID_0("%I3d", bad_format_string);
    CHECK_INVALID_0("%I32", bad_format_string);
    CHECK_INVALID_0("%I33", bad_format_string);
    CHECK_INVALID_0("%I6", bad_format_string);
    CHECK_INVALID_0("%I62", bad_format_string);
    CHECK_INVALID_0("%I63", bad_format_string);
    CHECK_INVALID_0("%I63d", bad_format_string);
    CHECK_INVALID_0("%I64", bad_format_string);
    CHECK_INVALID_0("%I128d", bad_format_string);
    CHECK_INVALID_0("%3.*4d", bad_format_string);

    // mixing positional and non-positional
    CHECK_INVALID_2("%1% %2d", 1, 2, bad_format_string);
    CHECK_INVALID_2("%2d %2%", 1, 2, bad_format_string);

    // found while improving coverage - a number following the * for width
    // or precision was being eaten instead of being treated as an error
    CHECK_INVALID_0("%1$*4d", bad_format_string);
    CHECK_INVALID_0("%1$05.*32d", bad_format_string);
    CHECK_INVALID_0("%1$05.*6", bad_format_string);

    // found while improving coverage, this caused an unhandled exception
    // the "T" conversion specifier didn't handle the exception flags properly
    CHECK_INVALID_0("%1$T", bad_format_string);       // missing fill character

    // test what() on exceptions
    format_error base_err;
    BOOST_CHECK_GT(strlen(base_err.what()), 0u);

    bad_format_string bfs(2, 3);
    BOOST_CHECK(boost::contains(bfs.what(), "bad_format_string"));

    too_few_args tfa(5, 4);
    BOOST_CHECK(boost::contains(tfa.what(), "format-string"));

    too_many_args tma(4, 5);
    BOOST_CHECK(boost::contains(tma.what(), "format-string"));

    boost::io::out_of_range oor(1, 2, 3);
    BOOST_CHECK(boost::contains(oor.what(), "out_of_range"));

    // bind and unbind
    boost::format two("%1%, %2%");
    two.bind_arg(1, 1);
    two.bind_arg(2, 2);
    BOOST_CHECK_EQUAL(two.str(), "1, 2");

    BOOST_CHECK_NO_THROW(two.clear_bind(1));
    BOOST_CHECK_THROW(two.str(), too_few_args);
    BOOST_CHECK_THROW(two.clear_bind(1), boost::io::out_of_range);
    two.exceptions(no_error_bits);
    BOOST_CHECK_NO_THROW(two.clear_bind(1));
    BOOST_CHECK_NO_THROW(two.str());

    return 0;
}

