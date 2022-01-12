///////////////////////////////////////////////////////////////////////////////
//  Copyright 2015 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/cpp_bin_float.hpp>
#include <type_traits>

//
// Move construct:
//
static_assert(std::is_nothrow_move_constructible<boost::multiprecision::cpp_bin_float_100>::value, "is_nothrow_move_constructible test");
//
// Move assign:
//
static_assert(std::is_nothrow_move_assignable<boost::multiprecision::cpp_bin_float_100>::value, "is_nothrow_move_assignable test");

//
// Construct:
//
static_assert(std::is_nothrow_default_constructible<boost::multiprecision::cpp_bin_float_100>::value, "is_nothrow_default_constructible test");
//
// Copy construct:
//
static_assert(std::is_nothrow_copy_constructible<boost::multiprecision::cpp_bin_float_100>::value, "is_nothrow_copy_constructible test");
//
// Assign:
//
static_assert(std::is_nothrow_copy_assignable<boost::multiprecision::cpp_bin_float_100>::value, "is_nothrow_copy_assignable test");

