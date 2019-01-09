// Copyright (c) 2014 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <cassert>
#include <typeinfo>
#include <boost/core/demangle.hpp>

#include <boost/safe_numerics/safe_compare.hpp>

template<class T1, class T2>
void print_argument_types(
    T1 v1,
    T2 v2
){
    const std::type_info & ti1 = typeid(v1);
    const std::type_info & ti2 = typeid(v2);

    std::cout
        << boost::core::demangle(ti1.name()) << ','
        << boost::core::demangle(ti2.name());
}

using namespace boost::safe_numerics;

template<class T1, class T2>
bool test_safe_compare_impl(
    T1 v1,
    T2 v2,
    char expected_result
){
    switch(expected_result){
    case '=': {
        if(! safe_compare::equal(v1, v2))
            return false;
        if(safe_compare::less_than(v1, v2))
            return false;
        if(safe_compare::greater_than(v1, v2))
            return false;
        break;
    }
    case '<': {
        if(! safe_compare::less_than(v1, v2))
            return false;
        if(safe_compare::greater_than(v1, v2))
            return false;
        if(safe_compare::equal(v1, v2))
            return false;
        break;
    }
    case '>':{
        if(! safe_compare::greater_than(v1, v2))
            return false;
        if(safe_compare::less_than(v1, v2))
            return false;
        if(safe_compare::equal(v1, v2))
            return false;
        break;
    }
    }
    return true;
}

template<class T1, class T2>
bool test_safe_compare(
    T1 v1,
    T2 v2,
    char expected_result
){
    print_argument_types(v1, v2);
    const bool result = test_safe_compare_impl(v1, v2, expected_result);
    if(! result)
        std::cout << " failed";
    std::cout << '\n';
    return result;
}

#include "test.hpp"
#include "test_values.hpp"

const char *test_compare_result[VALUE_ARRAY_SIZE] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ "=<>>=<>>=<>>=<>>=<<<=<<<=<<<=<<<>",
/* 1*/ ">=>>><>>><>>><>>>=<<><<<><<<><<<>",
/* 2*/ "<<=<<<><<<><<<><<<<<<<<<<<<<<<<<<",
/* 3*/ "<<>=<<>=<<>=<<>=<<<<<<<<<<<<<<<<<",
/* 4*/ "=<>>=<>>=<>>=<>>=<<<=<<<=<<<=<<<>",
/* 5*/ ">>>>>=>>><>>><>>>>>>>=<<><<<><<<>",
/* 6*/ "<<<<<<=<<<><<<><<<<<<<<<<<<<<<<<<",
/* 7*/ "<<>=<<>=<<>=<<>=<<<<<<<<<<<<<<<<<",

/* 8*/ "=<>>=<>>=<>>=<>>=<<<=<<<=<<<=<<<>",
/* 9*/ ">>>>>>>>>=>>><>>>>>>>>>>>=<<><<<>",
/*10*/ "<<<<<<<<<<=<<<><<<<<<<<<<<<<<<<<<",
/*11*/ "<<>=<<>=<<>=<<>=<<<<<<<<<<<<<<<<<",
/*12*/ "=<>>=<>>=<>>=<>>=<<<=<<<=<<<=<<<>",
/*13*/ ">>>>>>>>>>>>>=>>>>>>>>>>>>>>>=<<>",
/*14*/ "<<<<<<<<<<<<<<=<<<<<<<<<<<<<<<<<<",
/*15*/ "<<>=<<>=<<>=<<>=<<<<<<<<<<<<<<<<",

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
/*30*/ ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>=<>",
/*31*/ ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>=>",
/*32*/ "<<>><<>><<>><<>><<<<<<<<<<<<<<<<="
};

#define TEST_IMPL(v1, v2, result) \
    rval &= test_safe_compare(    \
        v1,                       \
        v2,                       \
        result                    \
    );
/**/

void break_check(unsigned int i, unsigned int j){
    std::cout << i << ',' << j << ',';
}

#define TESTX(value_index1, value_index2)          \
    break_check(value_index1, value_index2);       \
    TEST_IMPL(                                     \
        BOOST_PP_ARRAY_ELEM(value_index1, VALUES), \
        BOOST_PP_ARRAY_ELEM(value_index2, VALUES), \
        test_compare_result[value_index1][value_index2] \
    )
/**/

int main(int, char *[]){
    bool rval = true;
    TEST_EACH_VALUE_PAIR
    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
