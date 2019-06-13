#ifndef LIB2_THROW_HPP_INCLUDED
#define LIB2_THROW_HPP_INCLUDED

// Copyright 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/config.hpp>
#include <exception>

#if defined(LIB2_DYN_LINK)
# if defined(LIB2_SOURCE)
#  define LIB2_DECL BOOST_SYMBOL_EXPORT
# else
#  define LIB2_DECL BOOST_SYMBOL_IMPORT
# endif
#else
# define LIB2_DECL
#endif

namespace lib2
{

struct BOOST_SYMBOL_VISIBLE exception: public std::exception
{
};

LIB2_DECL void f();

} // namespace lib2

#endif // #ifndef LIB2_THROW_HPP_INCLUDED
