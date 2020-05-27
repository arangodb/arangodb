///////////////////////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/float128.hpp>
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
BOOST_STATIC_ASSERT(boost::is_nothrow_move_constructible<boost::multiprecision::float128>::value);

#endif

#if !defined(BOOST_NO_CXX11_NOEXCEPT) && !defined(BOOST_NO_SFINAE_EXPR) || defined(BOOST_IS_NOTHROW_MOVE_ASSIGN)
//
// Move assign:
//
BOOST_STATIC_ASSERT(boost::is_nothrow_move_assignable<boost::multiprecision::float128>::value);

#endif

//
// Construct:
//
#ifdef BOOST_HAS_NOTHROW_CONSTRUCTOR
BOOST_STATIC_ASSERT(boost::has_nothrow_constructor<boost::multiprecision::float128>::value);
#endif
//
// Copy construct:
//
#ifdef BOOST_HAS_NOTHROW_COPY
BOOST_STATIC_ASSERT(boost::has_nothrow_copy<boost::multiprecision::float128>::value);
#endif
//
// Assign:
//
#ifdef BOOST_HAS_NOTHROW_ASSIGN
BOOST_STATIC_ASSERT(boost::has_nothrow_assign<boost::multiprecision::float128>::value);
#endif

BOOST_STATIC_ASSERT(noexcept(boost::multiprecision::float128()));
BOOST_STATIC_ASSERT(noexcept(boost::multiprecision::float128(std::declval<const boost::multiprecision::float128&>())));
BOOST_STATIC_ASSERT(noexcept(boost::multiprecision::float128(std::declval<boost::multiprecision::float128>())));
BOOST_STATIC_ASSERT(noexcept(boost::multiprecision::float128(std::declval<const float128_type&>())));
BOOST_STATIC_ASSERT(noexcept(boost::multiprecision::float128(std::declval<float128_type>())));
BOOST_STATIC_ASSERT(noexcept(boost::multiprecision::float128(std::declval<const double&>())));
BOOST_STATIC_ASSERT(noexcept(boost::multiprecision::float128(std::declval<double>())));
BOOST_STATIC_ASSERT(noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<const boost::multiprecision::float128&>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<boost::multiprecision::float128>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<const float128_type&>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<float128_type>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<const double&>()));
BOOST_STATIC_ASSERT(noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<double>()));

struct any_convert
{
   template <class T>
   operator T ()const;   // Can throw!
};

BOOST_STATIC_ASSERT(!noexcept(boost::multiprecision::float128(std::declval<const any_convert&>())));
BOOST_STATIC_ASSERT(!noexcept(boost::multiprecision::float128(std::declval<any_convert>())));
BOOST_STATIC_ASSERT(!noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<const any_convert&>()));
BOOST_STATIC_ASSERT(!noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<any_convert>()));

#endif // noexcept


