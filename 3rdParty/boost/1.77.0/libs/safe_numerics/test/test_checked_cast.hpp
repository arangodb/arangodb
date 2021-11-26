#ifndef BOOST_SAFE_NUMERICS_TEST_CHECKED_CAST_HPP
#define BOOST_SAFE_NUMERICS_TEST_CHECKED_CAST_HPP

//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/mp11/integral.hpp>
#include "test_values.hpp"

// note: the types indexed on the left side of the table are gathered
// by filtering the test_values list.  So the types are in the same
// sequence

constexpr const char *test_result_cast[boost::mp11::mp_size<test_values>::value] = {
//      0       0       0       0
//      01234567012345670123456701234567
//      01234567890123456789012345678901
/* 0*/ ".....xx..xx..xx...xx.xxx.xxx.xxx",
/* 1*/ ".........xx..xx.......xx.xxx.xxx",
/* 2*/ ".............xx...........xx.xxx",
/* 3*/ "..............................xx",
/* 4*/ "..xx.xxx.xxx.xxx.....xxx.xxx.xxx",
/* 5*/ "..xx..xx.xxx.xxx.........xxx.xxx",
/* 6*/ "..xx..xx..xx.xxx.............xxx",
/* 7*/ "..xx..xx..xx..xx................"
};

#endif // BOOST_SAFE_NUMERICS_TEST_CHECKED_CAST_HPP
