#ifndef VISIBILITY_TEST_LIB_HPP_INCLUDED
#define VISIBILITY_TEST_LIB_HPP_INCLUDED

// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>
#include <iosfwd>

template <int Tag>
struct BOOST_SYMBOL_VISIBLE my_info
{
	int value;

    template <class CharT, class Traits>
    friend std::basic_ostream<CharT, Traits> & operator<<( std::basic_ostream<CharT, Traits> & os, my_info const & x )
    {
        return os << "Test my_info<" << Tag << ">::value = " << x.value;
    }
};

#endif
