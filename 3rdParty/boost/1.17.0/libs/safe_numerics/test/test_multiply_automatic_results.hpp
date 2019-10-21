#ifndef BOOST_SAFE_NUMERICS_TEST_MULTIPLY_AUTOMATIC_RESULTS_HPP
#define BOOST_SAFE_NUMERICS_TEST_MULTIPLY_AUTOMATIC_RESULTS_HPP

//  Copyright (c) 2019 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_values.hpp"

constexpr const char *test_multiplication_automatic_result[
    boost::mp11::mp_size<test_values>::value
] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ "..............................xx.",
/* 1*/ ".............xx..............xxx.",
/* 2*/ ".............xx..............xxx.",
/* 3*/ "..............x...............xx.",
/* 4*/ "..............................xx.",
/* 5*/ ".............xx..............xxx.",
/* 6*/ ".............xx..............xxx.",
/* 7*/ "..............x...............xx.",

/* 8*/ "..............................xx.",
/* 9*/ ".............xx..............xxx.",
/*10*/ ".............xx..............xxx.",
/*11*/ "..............x...............xx.",
/*12*/ "..............................xx.",
/*13*/ ".xx..xx..xx..xx..xxx.xxx.xxx.xxx.",
/*14*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*15*/ "..............x...............xx.",

//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/*16*/ ".................................",
/*17*/ ".............xx..............xxx.",
/*18*/ ".............xx..............xxx.",
/*19*/ ".............xx..............xxx.",
/*20*/ ".................................",
/*21*/ ".............xx..............xxx.",
/*22*/ ".............xx..............xxx.",
/*23*/ ".............xx..............xxx.",

/*24*/ ".................................",
/*25*/ ".............xx..............xxx.",
/*26*/ ".............xx..............xxx.",
/*27*/ ".............xx..............xxx.",
/*28*/ ".................................",
/*29*/ ".xx..xx..xx..xx..xxx.xxx.xxx.xxx.",
/*30*/ "xxxxxxxxxxxxxxxx.xxx.xxx.xxx.xxxx",
/*31*/ "xxxxxxxxxxxxxxxx.xxx.xxx.xxx.xxxx",
/*32*/ "..............................xx."
};

#endif // BOOST_SAFE_NUMERICS_TEST_MULTIPLY_AUTOMATIC_RESULTS_HPP
