//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <exception>

#include <boost/safe_numerics/safe_integer.hpp>

template <class T>
using safe_t = boost::safe_numerics::safe<T>;

#include "test_multiply.hpp"
#include "test.hpp"
#include "test_values.hpp"
#include "check_symmetry.hpp"

// note: These tables presume that the the size of an int is 32 bits.
// This should be changed for a different architecture or better yet
// be dynamically adjusted depending on the indicated architecture

const char *test_multiplication_result[VALUE_ARRAY_SIZE] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ ".................................",
/* 1*/ ".........xx..xx..........xxx.xxx.",
/* 2*/ ".........xx..xx.........xxxxxxxx.",
/* 3*/ "..........x...x.........xxxxxxxx.",
/* 4*/ ".................................",
/* 5*/ ".........xx..xx..........xxx.xxx.",
/* 6*/ ".........xx..xx.........xxxxxxxx.",
/* 7*/ "..........x...x.........xxxxxxxx.",

/* 8*/ ".................................",
/* 9*/ ".xx..xx..xx..xx..xxx.xxx.xxx.xxx.",
/*10*/ ".xxx.xxx.xxx.xx..xxx.xxxxxxxxxxx.",
/*11*/ "..........x...x.........xxxxxxxx.",
/*12*/ ".................................",
/*13*/ ".xx..xx..xx..xx..xxx.xxx.xxx.xxx.",
/*14*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxxxxxx.",
/*15*/ "..............x.............xxxx.",

//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/*16*/ ".................................",
/*17*/ ".........xx..xx..........xxx.xxx.",
/*18*/ ".........xx..xx..........xxx.xxx.",
/*19*/ ".........xx..xx..........xxx.xxx.",
/*20*/ ".................................",
/*21*/ ".........xx..xx..........xxx.xxx.",
/*22*/ ".........xx..xx..........xxx.xxx.",
/*23*/ ".........xx..xx........x.xxx.xxx.",

/*24*/ "..xx..xx..xx.....................",
/*25*/ ".xxx.xxx.xxx.xx..xxx.xxx.xxx.xxx.",
/*26*/ ".xxx.xxx.xxx.xx..xxx.xxx.xxx.xxx.",
/*27*/ ".xxx.xxx.xxx.xx..xxx.xxx.xxx.xxx.",
/*28*/ "..xx..xx..xx..xx.................",
/*29*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*30*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*31*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*31*/ "................................."
};

#define TEST_IMPL(v1, v2, result) \
    rval &= test_multiply(        \
        v1,                       \
        v2,                       \
        BOOST_PP_STRINGIZE(v1),   \
        BOOST_PP_STRINGIZE(v2),   \
        result                    \
    );
/**/

#define TESTX(value_index1, value_index2)          \
    (std::cout << value_index1 << ',' << value_index2 << ','); \
    TEST_IMPL(                                     \
        BOOST_PP_ARRAY_ELEM(value_index1, VALUES), \
        BOOST_PP_ARRAY_ELEM(value_index2, VALUES), \
        test_multiplication_result[value_index1][value_index2] \
    )
/**/

int main(int, char *[]){
    // sanity check on test matrix - should be symetrical
    
    for(int i = 0; i < VALUE_ARRAY_SIZE; ++i)
        for(int j = i + 1; j < VALUE_ARRAY_SIZE; ++j)
            if(!(test_multiplication_result[i][j]
            == test_multiplication_result[j][i]))
                std::cerr << "asymmetry in operations "
                    << "i == " << i
                    << " j == " << j
                    << std::endl;
    bool rval = true;
    TEST_EACH_VALUE_PAIR
    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
