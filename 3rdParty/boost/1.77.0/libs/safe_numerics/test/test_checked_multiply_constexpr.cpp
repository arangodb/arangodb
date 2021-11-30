//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/safe_numerics/checked_result.hpp>
#include <boost/safe_numerics/checked_result_operations.hpp>
#include <boost/safe_numerics/checked_integer.hpp>

template<class T>
constexpr bool test_checked_multiply(
    T v1,
    T v2,
    char expected_result
){
    using namespace boost::safe_numerics;
    const T result = v1 * v2;
    switch(expected_result){
    case '0':
    case '.':
        if(result.exception()){
            return false;
        }
        if(expected_result == '0'
        && result != T(0)
        ){
           return false;
        }
        return true;
    case '-':
        return safe_numerics_error::negative_overflow_error == result.m_e;
    case '+':
        return safe_numerics_error::positive_overflow_error == result.m_e;
    case '!':
        return safe_numerics_error::range_error == result.m_e;
    }
    return false;
}

#include "test_checked_multiply.hpp"

template<typename T, typename First, typename Second>
struct test_signed_pair {
    static const std::size_t i = First();
    static const std::size_t j = Second();
    // note: is constexpr really required here?  compilers disagree!
    constexpr static const bool value = test_checked_multiply(
        signed_values<T>[i],
        signed_values<T>[j],
        signed_multiplication_results[i][j]
    );
};

template<typename T, typename First, typename Second>
struct test_unsigned_pair {
    static const std::size_t i = First();
    static const std::size_t j = Second();
    // note: is constexpr really required here?  compilers disagree!
    constexpr static const bool value = test_checked_multiply(
        unsigned_values<T>[i],
        unsigned_values<T>[j],
        unsigned_multiplication_results[i][j]
    );
};

#include "check_symmetry.hpp"
#include <boost/mp11/algorithm.hpp>

int main(){
    static_assert(
        check_symmetry(signed_multiplication_results),
        "sanity check on test matrix - should be symmetrical"
    );
    static_assert(
        check_symmetry(unsigned_multiplication_results),
        "sanity check on test matrix - should be symmetrical"
    );
    using namespace boost::mp11;

    static_assert(
        mp_all_of<
            mp_product<
                test_signed_pair,
                signed_test_types,
                signed_value_indices, signed_value_indices
            >,
            mp_to_bool
        >(),
        "all values for all signed types correctly multiplied"
    );

    static_assert(
        mp_all_of<
            mp_product<
                test_unsigned_pair,
                unsigned_test_types,
                unsigned_value_indices, unsigned_value_indices
            >,
            mp_to_bool
        >(),
        "all values for all unsigned types correctly multiplied"
    );
    return 0;
}
