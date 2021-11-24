// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This program is an adaptation of the following Boost Outcome example:
// https://github.com/ned14/outcome/blob/master/doc/src/snippets/using_result.cpp

#include <boost/leaf.hpp>
#include <algorithm>
#include <ctype.h>
#include <string>
#include <iostream>

namespace leaf = boost::leaf;

enum class ConversionErrc
{
    EmptyString = 1,
    IllegalChar,
    TooLong
};

leaf::result<int> convert(const std::string& str) noexcept
{
    if (str.empty())
        return leaf::new_error(ConversionErrc::EmptyString);

    if (!std::all_of(str.begin(), str.end(), ::isdigit))
        return leaf::new_error(ConversionErrc::IllegalChar);

    if (str.length() > 9)
        return leaf::new_error(ConversionErrc::TooLong);

    return atoi(str.c_str());
}

// Do not static_store BigInt to actually work -- it's a stub.
struct BigInt
{
    static leaf::result<BigInt> fromString(const std::string& s) { return BigInt{s}; }
    explicit BigInt(const std::string&) { }
    BigInt half() const { return BigInt{""}; }
    friend std::ostream& operator<<(std::ostream& o, const BigInt&) { return o << "big int half"; }
};

// This function handles ConversionErrc::TooLong errors, forwards any other
// error to the caller.
leaf::result<void> print_half(const std::string& text)
{
    return leaf::try_handle_some(
        [&]() -> leaf::result<void>
        {
            BOOST_LEAF_AUTO(r,convert(text));
            std::cout << r / 2 << std::endl;
            return { };
        },
        [&]( leaf::match<ConversionErrc, ConversionErrc::TooLong> ) -> leaf::result<void>
        {
            BOOST_LEAF_AUTO(i, BigInt::fromString(text));
            std::cout << i.half() << std::endl;
            return { };
        } );
}

int main( int argc, char const * argv[] )
{
    return leaf::try_handle_all(
        [&]() -> leaf::result<int>
        {
            BOOST_LEAF_CHECK( print_half(argc<2 ? "" : argv[1]) );
            std::cout << "ok" << std::endl;
            return 0;
        },

        []( leaf::match<ConversionErrc, ConversionErrc::EmptyString> )
        {
            std::cerr << "Empty string!" << std::endl;
            return 1;
        },

        []( leaf::match<ConversionErrc, ConversionErrc::IllegalChar> )
        {
            std::cerr << "Illegal char!" << std::endl;
            return 2;
        },

        []( leaf::error_info const & unmatched )
        {
            // This will never execute in this program, but it would detect
            // logic errors where an unknown error reaches main. In this case,
            // we print diagnostic information.
            std::cerr <<
                "Unknown failure detected" << std::endl <<
                "Cryptic diagnostic information follows" << std::endl <<
                unmatched;
            return 3;
        } );
}

////////////////////////////////////////

#ifdef BOOST_LEAF_NO_EXCEPTIONS

namespace boost
{
    BOOST_LEAF_NORETURN void throw_exception( std::exception const & e )
    {
        std::cerr << "Terminating due to a C++ exception under BOOST_LEAF_NO_EXCEPTIONS: " << e.what();
        std::terminate();
    }

    struct source_location;
    BOOST_LEAF_NORETURN void throw_exception( std::exception const & e, boost::source_location const & )
    {
        throw_exception(e);
    }
}

#endif
