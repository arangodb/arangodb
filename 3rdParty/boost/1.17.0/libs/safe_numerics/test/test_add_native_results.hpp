#ifndef BOOST_SAFE_NUMERICS_TEST_ADD_NATIVE_RESULTS_HPP
#define BOOST_SAFE_NUMERICS_TEST_ADD_NATIVE_RESULTS_HPP

//  Copyright (c) 2019 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_values.hpp"

constexpr const char *test_addition_native_result[
    boost::mp11::mp_size<test_values>::value
] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ ".........x...x.............x...x.",
/* 1*/ ".........x...x.............x...x.",
/* 2*/ "..........x...x.........xxxxxxxx.",
/* 3*/ "..........x...x.........xxxxxxxx.",
/* 4*/ ".........x...x.............x...x.",
/* 5*/ ".........x...x.............x...x.",
/* 6*/ "..........x...x.........xxxxxxxx.",
/* 7*/ "..........x...x.........xxxxxxxx.",

/* 8*/ ".........x...x.............x...x.",
/* 9*/ "xx..xx..xx...x..xxxxxxxx...x...x.",
/*10*/ "..xx..xx..xx..x.........xxxxxxxx.",
/*11*/ "..........x...x.........xxxxxxxx.",
/*12*/ ".............x.................x.",
/*13*/ "xx..xx..xx..xx..xxxxxxxxxxxx...x.",
/*14*/ "..xx..xx..xx..xx............xxxx.",
/*15*/ "..............x.............xxxx.",

//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/*16*/ ".........x...x.............x...x.",
/*17*/ ".........x...x.............x...x.",
/*18*/ ".........x...x.............x...x.",
/*19*/ ".........x...x.............x...x.",
/*20*/ ".........x...x.............x...x.",
/*21*/ ".........x...x.............x...x.",
/*22*/ ".........x...x.............x...x.",
/*23*/ ".........x...x.............x...x.",

/*24*/ "..xx..xx..xx.x.............x...x.",
/*25*/ "..xx..xx..xx.x.............x...x.",
/*26*/ "..xx..xx..xx.x............xx...x.",
/*27*/ "xxxxxxxxxxxx.x..xxxxxxxxxxxx...x.",
/*28*/ "..xx..xx..xx..xx...............x.",
/*29*/ "..xx..xx..xx..xx...............x.",
/*30*/ "..xx..xx..xx..xx..............xx.",
/*31*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*32*/ "................................."
};

#endif	// BOOST_SAFE_NUMERICS_TEST_ADD_NATIVE_RESULTS_HPP

