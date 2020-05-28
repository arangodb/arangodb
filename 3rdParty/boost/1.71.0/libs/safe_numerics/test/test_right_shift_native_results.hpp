#ifndef BOOST_SAFE_NUMERICS_TEST_RIGHT_SHIFT_NATIVE_RESULTS_HPP
#define BOOST_SAFE_NUMERICS_TEST_RIGHT_SHIFT_NATIVE_RESULTS_HPP

//  Copyright (c) 2019 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_values.hpp"

constexpr const char *test_right_shift_native_result[
	boost::mp11::mp_size<test_values>::value
] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 1*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 2*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
/* 3*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
/* 4*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 5*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 6*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
/* 7*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",

/* 8*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 9*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*10*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
/*11*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
/*12*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*13*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*14*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
/*15*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",

//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/*16*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*17*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*18*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*19*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*20*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*21*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*22*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*23*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",

/*24*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*25*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*26*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*27*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*28*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*29*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*30*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*31*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*32*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx."
};

#endif	// BOOST_SAFE_NUMERICS_TEST_RIGHT_SHIFT_NATIVE_RESULTS_HPP
