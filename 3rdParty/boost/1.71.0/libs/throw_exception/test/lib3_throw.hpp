#ifndef LIB3_THROW_HPP_INCLUDED
#define LIB3_THROW_HPP_INCLUDED

// Copyright 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/config.hpp>
#include <exception>

#if defined(LIB3_DYN_LINK)
# if defined(LIB3_SOURCE)
#  define LIB3_DECL BOOST_SYMBOL_EXPORT
# else
#  define LIB3_DECL BOOST_SYMBOL_IMPORT
# endif
#else
# define LIB3_DECL
#endif

namespace lib3
{

struct BOOST_SYMBOL_VISIBLE exception: public std::exception
{
};

LIB3_DECL void f();

} // namespace lib3

#endif // #ifndef LIB3_THROW_HPP_INCLUDED
