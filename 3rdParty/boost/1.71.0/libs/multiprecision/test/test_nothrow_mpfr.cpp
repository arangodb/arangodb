///////////////////////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/mpfr.hpp>
#include <boost/type_traits/is_nothrow_move_constructible.hpp>
#include <boost/type_traits/is_nothrow_move_assignable.hpp>
#include <boost/type_traits/has_nothrow_constructor.hpp>
#include <boost/type_traits/has_nothrow_assign.hpp>
#include <boost/type_traits/has_nothrow_copy.hpp>
#include <boost/static_assert.hpp>

#ifndef BOOST_NO_CXX11_NOEXCEPT

#if !defined(BOOST_NO_CXX11_NOEXCEPT) && !defined(BOOST_NO_SFINAE_EXPR) || defined(BOOST_IS_NOTHROW_MOVE_CONSTRUCT)
//
// Move construct:
//
BOOST_STATIC_ASSERT(boost::is_nothrow_move_constructible<boost::multiprecision::mpfr_float>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_constructible<boost::multiprecision::mpfr_float_100>::value);

#endif

#if !defined(BOOST_NO_CXX11_NOEXCEPT) && !defined(BOOST_NO_SFINAE_EXPR) || defined(BOOST_IS_NOTHROW_MOVE_ASSIGN)
//
// Move assign:
//
BOOST_STATIC_ASSERT(boost::is_nothrow_move_assignable<boost::multiprecision::mpfr_float>::value);
BOOST_STATIC_ASSERT(boost::is_nothrow_move_assignable<boost::multiprecision::mpfr_float_100>::value);

#endif

#endif // noexcept


