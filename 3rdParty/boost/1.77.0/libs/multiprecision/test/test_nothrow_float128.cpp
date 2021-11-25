///////////////////////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/float128.hpp>
#include <type_traits>

//
// Move construct:
//
static_assert(std::is_nothrow_move_constructible<boost::multiprecision::float128>::value, "noexcept test");
//
// Move assign:
//
static_assert(std::is_nothrow_move_assignable<boost::multiprecision::float128>::value, "noexcept test");
//
// Construct:
//
static_assert(std::is_nothrow_default_constructible<boost::multiprecision::float128>::value, "noexcept test");
//
// Copy construct:
//
static_assert(std::is_nothrow_copy_constructible<boost::multiprecision::float128>::value, "noexcept test");
//
// Assign:
//
static_assert(std::is_nothrow_copy_assignable<boost::multiprecision::float128>::value, "noexcept test");

static_assert(noexcept(boost::multiprecision::float128()), "noexcept test");
static_assert(noexcept(boost::multiprecision::float128(std::declval<const boost::multiprecision::float128&>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::float128(std::declval<boost::multiprecision::float128>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::float128(std::declval<const float128_type&>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::float128(std::declval<float128_type>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::float128(std::declval<const double&>())), "noexcept test");
static_assert(noexcept(boost::multiprecision::float128(std::declval<double>())), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<const boost::multiprecision::float128&>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<boost::multiprecision::float128>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<const float128_type&>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<float128_type>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<const double&>()), "noexcept test");
static_assert(noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<double>()), "noexcept test");

struct any_convert
{
   template <class T>
   operator T() const; // Can throw!
};

static_assert(!noexcept(boost::multiprecision::float128(std::declval<const any_convert&>())), "noexcept test");
static_assert(!noexcept(boost::multiprecision::float128(std::declval<any_convert>())), "noexcept test");
static_assert(!noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<const any_convert&>()), "noexcept test");
static_assert(!noexcept(std::declval<boost::multiprecision::float128&>() = std::declval<any_convert>()), "noexcept test");

