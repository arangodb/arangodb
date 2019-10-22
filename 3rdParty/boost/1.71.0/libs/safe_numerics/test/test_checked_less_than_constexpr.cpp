//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/safe_numerics/checked_result.hpp>
#include <boost/safe_numerics/checked_result_operations.hpp>
#include <boost/safe_numerics/checked_integer.hpp>

// note: T should be of tyme checked_result<R> for some integer type R
template<class T>
constexpr bool test_checked_less_than(
    T v1,
    T v2,
    char expected_result
){
    using namespace boost::safe_numerics;
    const boost::logic::tribool result = v1 < v2;
    switch(expected_result){
    case '<':
        if(result)
            return true;
        break;
    case '>':
    case '=':
        if(!result)
            return true;
        break;
    case '!':
        if(indeterminate(result))
            return true;
        break;
    }
    return false;
}

#include "test_checked_comparison.hpp"

template<typename T, typename First, typename Second>
struct test_signed_pair {
    static const std::size_t i = First();
    static const std::size_t j = Second();
    // note: is constexpr really required here?  compilers disagree!
    constexpr static const bool value = test_checked_less_than(
        signed_values<T>[i],
        signed_values<T>[j],
        signed_comparison_results[i][j]
    );
};

template<typename T, typename First, typename Second>
struct test_unsigned_pair {
    static const std::size_t i = First();
    static const std::size_t j = Second();
    // note: is constexpr really required here?  compilers disagree!
    constexpr static const bool value = test_checked_less_than(
        unsigned_values<T>[i],
        unsigned_values<T>[j],
        unsigned_comparison_results[i][j]
    );
};

#include <boost/mp11/algorithm.hpp>

int main(){
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
        "all values for all signed types correctly compared"
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
        "all values for all unsigned types correctly compared"
    );
    return 0;
}
