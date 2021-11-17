//  Copyright (c) 2017 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// test construction assignments

#include <iostream>

#include <boost/safe_numerics/safe_integer.hpp>

template <class T>
using safe_t = boost::safe_numerics::safe<
    T,
    boost::safe_numerics::native
>;

#include <boost/mp11/list.hpp>
#include <boost/mp11/algorithm.hpp>
#include "test_values.hpp"

// note: same test matrix as used in test_checked.  Here we test all combinations
// safe and unsafe integers.  in test_checked we test all combinations of
// integer primitives

const char *test_assignment_result[boost::mp11::mp_size<test_values>::value] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ ".....xx..xx..xx...xx.xxx.xxx.xxx.",
/* 1*/ ".....xx..xx..xx...xx.xxx.xxx.xxx.",
/* 2*/ ".....xx..xx..xx...xx.xxx.xxx.xxx.",
/* 3*/ ".....xx..xx..xx...xx.xxx.xxx.xxx.",
/* 4*/ ".........xx..xx.......xx.xxx.xxx.",
/* 5*/ ".........xx..xx.......xx.xxx.xxx.",
/* 6*/ ".........xx..xx.......xx.xxx.xxx.",
/* 7*/ ".........xx..xx.......xx.xxx.xxx.",

/* 8*/ ".............xx...........xx.xxx.",
/* 9*/ ".............xx...........xx.xxx.",
/*10*/ ".............xx...........xx.xxx.",
/*11*/ ".............xx...........xx.xxx.",
/*12*/ "..............................xx.",
/*13*/ "..............................xx.",
/*14*/ "..............................xx.",
/*15*/ "..............................xx.",

//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/*16*/ "..xx.xxx.xxx.xxx.....xxx.xxx.xxx.",
/*17*/ "..xx.xxx.xxx.xxx.....xxx.xxx.xxx.",
/*18*/ "..xx.xxx.xxx.xxx.....xxx.xxx.xxx.",
/*19*/ "..xx.xxx.xxx.xxx.....xxx.xxx.xxx.",
/*20*/ "..xx..xx.xxx.xxx.........xxx.xxx.",
/*21*/ "..xx..xx.xxx.xxx.........xxx.xxx.",
/*22*/ "..xx..xx.xxx.xxx.........xxx.xxx.",
/*23*/ "..xx..xx.xxx.xxx.........xxx.xxx.",

/*24*/ "..xx..xx..xx.xxx.............xxx.",
/*25*/ "..xx..xx..xx.xxx.............xxx.",
/*26*/ "..xx..xx..xx.xxx.............xxx.",
/*27*/ "..xx..xx..xx.xxx.............xxx.",
/*28*/ "..xx..xx..xx..xx.................",
/*29*/ "..xx..xx..xx..xx.................",
/*30*/ "..xx..xx..xx..xx.................",
/*31*/ "..xx..xx..xx..xx.................",
//      012345678901234567890123456789012
/*32*/ ".....xx..xx..xx...xx.xxx.xxx.xxx."
};

#include <boost/mp11/algorithm.hpp>
#include <boost/core/demangle.hpp>

template <class T>
using safe_t = boost::safe_numerics::safe<
    T,
    boost::safe_numerics::native
>;
#include "test_assignment.hpp"

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
        m_error &= test_assignment<T1, T2>(
            boost::mp11::mp_at_c<L, i1>(), // value of first argument
            boost::mp11::mp_at_c<L, i2>(), // value of second argument
            boost::core::demangle(typeid(T1).name()).c_str(),
            boost::core::demangle(typeid(T2).name()).c_str(),
            test_assignment_result[i1][i2]
        );
    }
};

int main(int, char *[]){

    // TEST_EACH_VALUE_PAIR
    test<test_values> rval(true);

    using value_indices = mp_iota_c<mp_size<test_values>::value>;
    mp_for_each<
        mp_product<mp_list, value_indices, value_indices>
    >(rval);

    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
