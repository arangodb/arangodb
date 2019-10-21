#ifndef BOOST_SAFE_NUMERICS_TEST_ADD_AUTOMATIC_RESULTS_HPP
#define BOOST_SAFE_NUMERICS_TEST_ADD_AUTOMATIC_RESULTS_HPP

//  Copyright (c) 2019 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_values.hpp"

// note: same test matrix as used in test_checked.  Here we test all combinations
// safe and unsafe integers.  in test_checked we test all combinations of
// integer primitives

constexpr const char *test_subtraction_automatic_result[
	boost::mp11::mp_size<test_values>::value
] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ "..............x...............xx.",
/* 1*/ "..............x...............xx.",
/* 2*/ ".............x...............xxx.",
/* 3*/ "..............................xx.",
/* 4*/ "..............x...............xx.",
/* 5*/ "..............x...............xx.",
/* 6*/ ".............x...............xxx.",
/* 7*/ "..............................xx.",

/* 8*/ "..............x...............xx.",
/* 9*/ "..............x...............xx.",
/*10*/ ".............x...............xxx.",
/*11*/ "..............................xx.",
/*12*/ "..............x...............xx.",
/*13*/ "..xx..xx..xx..xx..............xx.",
/*14*/ "xx..xx..xx..xx..xxxxxxxxxxxxxxxx.",
/*15*/ "..............................xx.",

//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/*16*/ "..............x...............xx.",
/*17*/ "..............x...............xx.",
/*18*/ "..............x...............xx.",
/*19*/ "..............x...............xx.",
/*20*/ "..............x...............xx.",
/*21*/ "..............x...............xx.",
/*22*/ "..............x...............xx.",
/*23*/ "..............x...............xx.",

/*24*/ "..............x...............xx.",
/*25*/ "..............x...............xx.",
/*26*/ "..............x...............xx.",
/*27*/ "..............x...............xx.",
/*28*/ "..............x...............xx.",
/*29*/ "..xx..xx..xx..xx..............xx.",
/*30*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
/*31*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
/*32*/ "..............x...............xx."
};


#endif

