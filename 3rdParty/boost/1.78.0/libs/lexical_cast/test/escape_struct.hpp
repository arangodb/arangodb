//  See http://www.boost.org for most recent version, including documentation.
//
//  Copyright Antony Polukhin, 2020.
//
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt).

#ifndef BOOST_LEXICAL_CAST_TEST_ESCAPE_STRUCT_HPP_
#define BOOST_LEXICAL_CAST_TEST_ESCAPE_STRUCT_HPP_

#include <istream>
#include <ostream>

struct EscapeStruct
{
    EscapeStruct() {}
    EscapeStruct(const std::string& s)
        : str_(s)
    {}

    std::string str_;
};

inline std::ostream& operator<< (std::ostream& o, const EscapeStruct& rhs)
{
    return o << rhs.str_;
}

inline std::istream& operator>> (std::istream& i, EscapeStruct& rhs)
{
    return i >> rhs.str_;
}


#endif // BOOST_LEXICAL_CAST_TEST_ESCAPE_STRUCT_HPP_
