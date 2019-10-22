#ifndef BOOST_TEST_ADD_CONSTEXPR_HPP
#define BOOST_TEST_ADD_CONSTEXPR_HPP

//  Copyright (c) 2015 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/safe_numerics/safe_integer.hpp>

template<class T1, class T2>
constexpr bool test_modulus_constexpr(
    T1 v1,
    T2 v2,
    char expected_result
){
    using namespace boost::safe_numerics;
    // if we don't expect the operation to pass, we can't
    // check the constexpr version of the calculation so
    // just return success.
    if(expected_result == 'x')
        return true;
    safe_t<T1>(v1) % v2;
    v1 % safe_t<T2>(v2);
    safe_t<T1>(v1) % safe_t<T2>(v2);
    return true; // correct result
}

#endif // BOOST_TEST_ADD_CONSTEXPR_HPP
