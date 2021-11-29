
// Copyright 1998-2002 John Maddock
// Copyright 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

// See library home page at http://www.boost.org/libs/regex

#include <boost/regex.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>

bool validate_card_format(const std::string& s)
{
    static const boost::regex e("(\\d{4}[- ]){3}\\d{4}");
    return boost::regex_match(s, e);
}

const boost::regex card_rx("\\A(\\d{3,4})[- ]?(\\d{4})[- ]?(\\d{4})[- ]?(\\d{4})\\z");
const std::string machine_format("\\1\\2\\3\\4");
const std::string human_format("\\1-\\2-\\3-\\4");

std::string machine_readable_card_number(const std::string& s)
{
    return boost::regex_replace(s, card_rx, machine_format, boost::match_default | boost::format_sed);
}

std::string human_readable_card_number(const std::string& s)
{
    return boost::regex_replace(s, card_rx, human_format, boost::match_default | boost::format_sed);
}

int main()
{
    std::string s[ 4 ] = { "0000111122223333", "0000 1111 2222 3333", "0000-1111-2222-3333", "000-1111-2222-3333" };

    BOOST_TEST( !validate_card_format( s[0] ) );
    BOOST_TEST_EQ( machine_readable_card_number( s[0] ), s[0] );
    BOOST_TEST_EQ( human_readable_card_number( s[0] ), s[2] );

    BOOST_TEST( validate_card_format( s[1] ) );
    BOOST_TEST_EQ( machine_readable_card_number( s[1] ), s[0] );
    BOOST_TEST_EQ( human_readable_card_number( s[1] ), s[2] );

    BOOST_TEST( validate_card_format( s[2] ) );
    BOOST_TEST_EQ( machine_readable_card_number( s[2] ), s[0] );
    BOOST_TEST_EQ( human_readable_card_number( s[2] ), s[2] );

    BOOST_TEST( !validate_card_format( s[3] ) );

    return boost::report_errors();
}
