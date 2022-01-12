
// Copyright 1998-2002 John Maddock
// Copyright 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

// See library home page at http://www.boost.org/libs/regex

#include <boost/regex/icu.hpp>
#include <cassert>
#include <string>

bool validate_card_format(const std::string& s)
{
    static const boost::u32regex e = boost::make_u32regex("(\\d{4}[- ]){3}\\d{4}");
    return boost::u32regex_match(s, e);
}

const boost::u32regex card_rx = boost::make_u32regex("\\A(\\d{3,4})[- ]?(\\d{4})[- ]?(\\d{4})[- ]?(\\d{4})\\z");
const std::string machine_format("\\1\\2\\3\\4");
const std::string human_format("\\1-\\2-\\3-\\4");

std::string machine_readable_card_number(const std::string& s)
{
    return boost::u32regex_replace(s, card_rx, machine_format, boost::match_default | boost::format_sed);
}

std::string human_readable_card_number(const std::string& s)
{
    return boost::u32regex_replace(s, card_rx, human_format, boost::match_default | boost::format_sed);
}

int main()
{
    std::string s[ 4 ] = { "0000111122223333", "0000 1111 2222 3333", "0000-1111-2222-3333", "000-1111-2222-3333" };

    assert( !validate_card_format( s[0] ) );
    assert( machine_readable_card_number( s[0] ) == s[0] );
    assert( human_readable_card_number( s[0] ) == s[2] );

    assert( validate_card_format( s[1] ) );
    assert( machine_readable_card_number( s[1] ) == s[0] );
    assert( human_readable_card_number( s[1] ) == s[2] );

    assert( validate_card_format( s[2] ) );
    assert( machine_readable_card_number( s[2] ) == s[0] );
    assert( human_readable_card_number( s[2] ) == s[2] );

    assert( !validate_card_format( s[3] ) );

    return 0;
}
