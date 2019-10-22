// Copyright (c) 2014 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

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

#include <boost/mp11/algorithm.hpp>

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

#include "test_values.hpp"

const char *test_compare_result[boost::mp11::mp_size<test_values>::value] = {
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

using namespace boost::mp11;

template<typename L>
struct test {
    static_assert(mp_is_list<L>(), "must be a list of integral constants");
    bool m_error;
    test(bool b = true) : m_error(b) {}
    operator bool(){
        return m_error;
    }
    template<typename T>
    void operator()(const T &){
        static_assert(mp_is_list<T>(), "must be a list of two integral constants");
        constexpr size_t i1 = mp_first<T>(); // index of first argument
        constexpr size_t i2 = mp_second<T>();// index of second argument
        std::cout << i1 << ',' << i2 << ',';
        using T1 = typename boost::mp11::mp_at_c<L, i1>::value_type;
        using T2 = typename boost::mp11::mp_at_c<L, i2>::value_type;
        m_error &= test_safe_compare<T1, T2>(
            boost::mp11::mp_at_c<L, i1>(), // value of first argument
            boost::mp11::mp_at_c<L, i2>(), // value of second argument
            test_compare_result[i1][i2]
        );
    }
};

int main(){
    //TEST_EACH_VALUE_PAIR
    test<test_values> rval(true);

    using value_indices = mp_iota_c<mp_size<test_values>::value>;
    mp_for_each<
        mp_product<mp_list, value_indices, value_indices>
    >(rval);

    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
