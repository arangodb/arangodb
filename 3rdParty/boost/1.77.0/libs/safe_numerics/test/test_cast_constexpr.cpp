//  Copyright (c) 2019 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// compile only test to test constexpr casting

#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/native.hpp>
#include <boost/safe_numerics/exception_policies.hpp>

template <class T>
using safe_t = boost::safe_numerics::safe<
    T,
    boost::safe_numerics::native,
    boost::safe_numerics::trap_exception
>;

constexpr const char * test_casting_results[] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ ".....xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 1*/ ".........xxx.xxx...x.xxx.xxx.xxx.",
/* 2*/ ".............xxx...x...x.xxx.xxx.",
/* 3*/ "...................x...x...x.xxx.",
/* 4*/ "..xx.xxx.xxx.xxx..xx.xxx.xxx.xxx.",
/* 5*/ "..xx..xx.xxx.xxx......xx.xxx.xxx.",
/* 6*/ "..xx..xx..xx.xxx..........xx.xxx.",
/* 7*/ "..xx..xx..xx..xx..............xx.",
};

#include <boost/safe_numerics/safe_integer_literal.hpp>
using namespace boost::safe_numerics;

#include <boost/mp11/algorithm.hpp>

using namespace boost::mp11;

template<class T>
struct p {
    constexpr static bool value = '.' == test_casting_results[mp_first<T>::value][mp_second<T>::value];
};

template<class T2, class T1>
constexpr bool test_cast_constexpr(const T1 & v1){
    // if we don't expect the operation to pass, we can't
    // check the constexpr version of the calculation so
    // just return success.
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-value"
    static_cast<safe_t<T2>>(v1);
    static_cast<T2>(v1);
    #pragma GCC diagnostic pop
    return true;
}

#include "test_values.hpp"

template<typename L2>
struct test {
    static_assert(mp_is_list<L2>(), "must be a list of two indices");
    const static std::size_t i = mp_first<L2>();
    const static std::size_t j = mp_second<L2>();
    using T = mp_at_c<test_types, i>; // first element is a type
    using T1 = typename mp_at_c<test_values, j>::value_type;
    const static T1 v = mp_at_c<test_values, j>::value;
    const static bool value =
        test_cast_constexpr<T>(make_safe_literal(v, native, trap_exception));
};

int main(){
    using namespace boost::safe_numerics;

    using type_indices = mp_iota_c<mp_size<test_types>::value>;
    using value_indices = mp_iota_c<mp_size<test_values>::value>;

    // generate all combinations of types <- value
    using l = mp_product<mp_list,type_indices,value_indices>;
    //boost::safe_numerics::utility::print_types<l> lp;

    // filter out the invalid ones
    using l1 = mp_copy_if<l, p>;
    //boost::safe_numerics::utility::print_types<l1> l1p;

    // verify that all valid ones compile without error
    static_assert(mp_all_of<l1, test>(), "testing all valid casts");
    return 0;
}
