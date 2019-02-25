//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <exception>

#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/automatic.hpp>

template <class T>
using safe_t = boost::safe_numerics::safe<
    T,
    boost::safe_numerics::automatic
>;

#include "test_divide.hpp"
#include "test.hpp"
#include "test_values.hpp"

// note: These tables presume that the the size of an int is 32 bits.
// This should be changed for a different architecture or better yet
// be dynamically adjusted depending on the indicated architecture

const char *test_division_result[VALUE_ARRAY_SIZE] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ "..............................xxx",
/* 1*/ "..............................xxx",
/* 2*/ "..............................xxx",
/* 3*/ "..............................xxx",
/* 4*/ "..............................xxx",
/* 5*/ "..............................xxx",
/* 6*/ "..............................xxx",
/* 7*/ "..............................xxx",

/* 8*/ "..............................xxx",
/* 9*/ "..............................xxx",
/*10*/ "..............................xxx",
/*11*/ "..............................xxx",
/*12*/ "..............................xxx",
/*13*/ "..............................xxx",
/*14*/ "...x...x...x...x..............xxx",
/*15*/ "..............................xxx",

//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/*16*/ "................................x",
/*17*/ "................................x",
/*18*/ "................................x",
/*19*/ "................................x",
/*20*/ "................................x",
/*21*/ "................................x",
/*22*/ "................................x",
/*23*/ "................................x",

/*24*/ "................................x",
/*25*/ "................................x",
/*26*/ "................................x",
/*27*/ "................................x",
/*28*/ "................................x",
/*29*/ "................................x",
/*30*/ "xxxxxxxxxxxxxxxx................x",
/*31*/ "xxxxxxxxxxxxxxxx................x",
/*32*/ "..............................xxx"
};

#define TEST_IMPL(v1, v2, result) \
    rval &= test_divide(          \
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
        test_division_result[value_index1][value_index2] \
    )
/**/

int main(int, char *[]){
    bool rval = true;
    //TESTX(0, 30);
    TEST_EACH_VALUE_PAIR
    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
