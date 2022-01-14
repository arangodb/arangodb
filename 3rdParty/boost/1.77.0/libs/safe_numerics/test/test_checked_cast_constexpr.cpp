//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include <boost/safe_numerics/checked_integer.hpp>

// test conversion to T2 from different literal types
template<class T2, class T1>
constexpr bool test_cast_constexpr(
    const T1 & v1,
    char result
){
    const boost::safe_numerics::checked_result<T2> r2 =
        boost::safe_numerics::checked::cast<T2>(v1);
    return ('x' == result) ?
        r2.exception() :
        ! r2.exception();
}

#include "test_checked_cast.hpp"

#include <boost/mp11/algorithm.hpp> // mp_iota
#include <boost/mp11/list.hpp> // mp_first, mp_second, mp_size, mp_is_list

using namespace boost::mp11;

// given a list of integral constants I, return a list of values
template<typename I>
struct get_values {
    static_assert(mp_is_list<I>(), "must be a list of two types");
    static_assert(2 == mp_size<I>::value, "must be a list of two types");
    constexpr static const size_t first = mp_first<I>(); // index of first argument
    constexpr static const size_t second = mp_second<I>();// index of second argument
};

template<typename I>
struct test_pair_constexpr {
    using pair = get_values<I>;
    using TResult = mp_at<test_types, mp_first<I>>;
    using TArg = typename mp_at<test_values, mp_second<I>>::value_type;
    constexpr static const TArg v = mp_at<test_values, mp_second<I>>()();
    constexpr static const bool value = test_cast_constexpr<TResult>(
        v,
        test_result_cast[pair::first][pair::second]
    );
};

int main(){
    // list of indices for values (integral constants)
    using value_indices = mp_iota_c<mp_size<test_values>::value>;
    // list of indices for types (integral constants)
    using type_indices = mp_iota_c<mp_size<test_types>::value>;
    // all combinations of type index, value index
    using index_pairs = mp_product<mp_list, type_indices, value_indices>;
    // test all type/value pairs to verify that cast can be done.
    static_assert(
        mp_all_of<index_pairs, test_pair_constexpr>::value,
        "all type/value pairs correctly cast"
    );
    return 0;
}
