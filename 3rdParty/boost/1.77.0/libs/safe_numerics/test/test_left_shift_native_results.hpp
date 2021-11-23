#ifndef BOOST_SAFE_NUMERICS_TEST_LEFT_SHIFT_NATIVE_RESULTS_HPP
#define BOOST_SAFE_NUMERICS_TEST_LEFT_SHIFT_NATIVE_RESULTS_HPP

//  Copyright (c) 2019 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_values.hpp"

constexpr const char *test_left_shift_native_result[
	boost::mp11::mp_size<test_values>::value
] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 1*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 2*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/* 3*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/* 4*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 5*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 6*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/* 7*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",

/* 8*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 9*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*10*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*11*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*12*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*13*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*14*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*15*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",

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
/*26*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*27*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*28*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*29*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*30*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*31*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*32*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx."
};

#endif // BOOST_SAFE_NUMERICS_TEST_LEFT_SHIFT_NATIVE_RESULTS_HPP

