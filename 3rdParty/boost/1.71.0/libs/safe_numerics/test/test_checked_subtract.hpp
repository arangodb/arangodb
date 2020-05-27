#ifndef BOOST_SAFE_NUMERICS_TEST_CHECKED_AND_HPP
#define BOOST_SAFE_NUMERICS_TEST_CHECKED_AND_HPP

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

constexpr const char * signed_subtraction_results[] = {
//      012345678
/* 0*/ "!!!!!!!!!",
/* 1*/ "!!!!!!!!!",
/* 2*/ "!!+++++++",
/* 3*/ "!!-...++-",
/* 4*/ "---....++",
/* 5*/ "---....++",
/* 6*/ "---.....+",
/* 7*/ "!!---...-",
/* 8*/ "!!-------",
};

constexpr const char * unsigned_subtraction_results[] = {
//      0123456
/* 0*/ "!!!!!!!",
/* 1*/ "!!!!!!!",
/* 2*/ "!!+++++",
/* 3*/ "!!-...+",
/* 4*/ "----..+",
/* 5*/ "-----.+",
/* 6*/ "------+",
};

#endif // BOOST_SAFE_NUMERICS_TEST_CHECKED_AND_HPP
