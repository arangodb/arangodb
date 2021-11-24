//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/automatic.hpp>
#include "test_multiply_automatic_results.hpp"

template <class T>
using safe_t = boost::safe_numerics::safe<
    T,
    boost::safe_numerics::automatic
>;
#include "test_multiply.hpp"

#include <boost/mp11/list.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/core/demangle.hpp>

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
        using T1 = typename mp_at_c<L, i1>::value_type;
        using T2 = typename mp_at_c<L, i2>::value_type;
        m_error &= test_multiply(
            mp_at_c<L, i1>()(), // value of first argument
            mp_at_c<L, i2>()(), // value of second argument
            boost::core::demangle(typeid(T1).name()).c_str(),
            boost::core::demangle(typeid(T2).name()).c_str(),
            test_multiplication_automatic_result[i1][i2]
        );
    }
};

#include "check_symmetry.hpp"

int main(){
    static_assert(
        check_symmetry(test_multiplication_automatic_result),
        "sanity check on test matrix - should be symmetrical"
    );

    test<test_values> rval(true);

    using value_indices = mp_iota_c<mp_size<test_values>::value>;
    mp_for_each<
        mp_product<mp_list, value_indices, value_indices>
    >(rval);

    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
