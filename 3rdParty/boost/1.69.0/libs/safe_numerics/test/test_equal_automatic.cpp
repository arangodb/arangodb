//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <exception>
#include <cassert>

#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/automatic.hpp>

template <class T>
using safe_t = boost::safe_numerics::safe<
    T,
    boost::safe_numerics::automatic
>;

#include "test_equal.hpp"
#include "test.hpp"
#include "test_values.hpp"

const char *test_equal_result[VALUE_ARRAY_SIZE] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ "=<>>=<>>=<>>=<>>=<<<=<<<=<<<=<xx>",
/* 1*/ ">=>>><>>><>>><>>>=<<><<<><<<><xx>",
/* 2*/ "<<=<<<><<<><<<><<<<<<<<<<<<<<<xx<",
/* 3*/ "<<>=<<>=<<>=<<>=<<<<<<<<<<<<<<xx<",
/* 4*/ "=<>>=<>>=<>>=<>>=<<<=<<<=<<<=<xx>",
/* 5*/ ">>>>>=>>><>>><>>>>>>>=<<><<<><xx>",
/* 6*/ "<<<<<<=<<<><<<><<<<<<<<<<<<<<<xx<",
/* 7*/ "<<>=<<>=<<>=<<>=<<<<<<<<<<<<<<xx<",

/* 8*/ "=<>>=<>>=<>>=<>>=<<<=<<<=<<<=<xx>",
/* 9*/ ">>>>>>>>>=>>><>>>>>>>>>>>=<<><xx>",
/*10*/ "<<<<<<<<<<=<<<><<<<<<<<<<<<<<<xx<",
/*11*/ "<<>=<<>=<<>=<<>=<<<<<<<<<<<<<<xx<",
/*12*/ "=<>>=<>>=<>>=<>>=<<<=<<<=<<<=<xx>",
/*13*/ ">>>>>>>>>>>>>=>>>>>>>>>>>>>>>=xx>",
/*14*/ "<<<<<<<<<<<<<<=<<<<<<<<<<<<<<<xx<",
/*15*/ "<<>=<<>=<<>=<<>=<<<<<<<<<<<<<<xx<",

//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/*16*/ "=<>>=<>>=<>>=<>>=<<<=<<<=<<<=<<<>",
/*17*/ ">=>>><>>><>>><>>>=<<><<<><<<><<<>",
/*18*/ ">>>>><>>><>>><>>>>=<><<<><<<><<<>",
/*19*/ ">>>>><>>><>>><>>>>>=><<<><<<><<<>",
/*20*/ "=<>>=<>>=<>>=<>>=<<<=<<<=<<<=<<<>",
/*21*/ ">>>>>=>>><>>><>>>>>>>=<<><<<><<<>",
/*22*/ ">>>>>>>>><>>><>>>>>>>>=<><<<><<<>",
/*23*/ ">>>>>>>>><>>><>>>>>>>>>=><<<><<<>",

/*24*/ "=<>>=<>>=<>>=<>>=<<<=<<<=<<<=<<<>",
/*25*/ ">>>>>>>>>=>>><>>>>>>>>>>>=<<><<<>",
/*26*/ ">>>>>>>>>>>>><>>>>>>>>>>>>=<><<<>",
/*27*/ ">>>>>>>>>>>>><>>>>>>>>>>>>>=><<<>",
/*28*/ "=<>>=<>>=<>>=<>>=<<<=<<<=<<<=<<<>",
/*29*/ ">>>>>>>>>>>>>=>>>>>>>>>>>>>>>=<<>",
/*30*/ "xxxxxxxxxxxxxxxx>>>>>>>>>>>>>>=<x",
/*31*/ "xxxxxxxxxxxxxxxx>>>>>>>>>>>>>>>=x",
/*32*/ "<<>><<>><<>><<>><<<<<<<<<<<<<<xx="
};

#define TEST_IMPL(v1, v2, result) \
    rval &= test_equal(           \
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
        test_equal_result[value_index1][value_index2] \
    )
/**/

int main(int, char *[]){
    bool rval = true;
    TEST_EACH_VALUE_PAIR
    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
