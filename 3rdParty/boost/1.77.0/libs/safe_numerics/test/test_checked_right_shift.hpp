#ifndef BOOST_SAFE_NUMERICS_TEST_CHECKED_RIGHT_SHIFT_HPP
#define BOOST_SAFE_NUMERICS_TEST_CHECKED_RIGHT_SHIFT_HPP

//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_checked_values.hpp"

// test result matrices

// key
// . success
// - negative_overflow_error
// + positive_overflow_error
// ? range_error
// n negative_shift,             // negative value in shift operator
// s negative_value_shift,       // shift a negative value
// l shift_too_large,            // l/r shift exceeds variable size

constexpr char const * const signed_right_shift_results[] = {
//      012345678
/* 0*/ "!!!!!!!!!",
/* 1*/ "!!!!!!!!!",
/* 2*/ "!!!++++++",
/* 3*/ "!!....+++",
/* 4*/ "!!.....++",
/* 5*/ ".........",
/* 6*/ "!!.....--",
/* 7*/ "!!....---",
/* 8*/ "!!!------",
};

constexpr char const * const unsigned_right_shift_results[] = {
//      0123456
/* 0*/ "!!!!!!!",
/* 1*/ "!!!!!!!",
/* 2*/ "!!!++++",
/* 3*/ "!!....+",
/* 4*/ "!!....+",
/* 5*/ ".......",
/* 6*/ "!!!----",
};

#endif // BOOST_SAFE_NUMERICS_TEST_CHECKED_RIGHT_SHIFT_HPP
