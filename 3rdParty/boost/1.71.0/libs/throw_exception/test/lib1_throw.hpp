#ifndef LIB1_THROW_HPP_INCLUDED
#define LIB1_THROW_HPP_INCLUDED

// Copyright 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/config.hpp>
#include <exception>

#if defined(LIB1_DYN_LINK)
# if defined(LIB1_SOURCE)
#  define LIB1_DECL BOOST_SYMBOL_EXPORT
# else
#  define LIB1_DECL BOOST_SYMBOL_IMPORT
# endif
#else
# define LIB1_DECL
#endif

namespace lib1
{

struct BOOST_SYMBOL_VISIBLE exception: public std::exception
{
};

LIB1_DECL void f();

} // namespace lib1

#endif // #ifndef LIB1_THROW_HPP_INCLUDED
