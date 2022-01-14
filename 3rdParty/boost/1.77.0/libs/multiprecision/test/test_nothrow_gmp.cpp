///////////////////////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/gmp.hpp>
#include <type_traits>

//
// Move construct:
//
static_assert(std::is_nothrow_move_constructible<boost::multiprecision::mpz_int>::value, "noexcept test");
static_assert(std::is_nothrow_move_constructible<boost::multiprecision::mpq_rational>::value, "noexcept test");
static_assert(std::is_nothrow_move_constructible<boost::multiprecision::mpf_float>::value, "noexcept test");
//
// Move assign:
//
static_assert(std::is_nothrow_move_assignable<boost::multiprecision::mpz_int>::value, "noexcept test");
static_assert(std::is_nothrow_move_assignable<boost::multiprecision::mpq_rational>::value, "noexcept test");
static_assert(std::is_nothrow_move_assignable<boost::multiprecision::mpf_float>::value, "noexcept test");
