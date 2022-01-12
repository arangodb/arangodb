//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include "test_checked_cast.hpp"
#include <boost/safe_numerics/checked_integer.hpp>

// test conversion to TResult from different literal types
template<class TResult, class TArg>
bool test_cast(
    const TArg & v,
    const char *tresult_name,
    const char *targ_name,
    char expected_result
){
    std::cout
        << "testing static_cast<" << tresult_name << ">(" << targ_name << ")"
        << std::endl;

    boost::safe_numerics::checked_result<TResult> r2 =
        boost::safe_numerics::checked::cast<TResult>(v);

    if(expected_result == 'x' && ! r2.exception()){
        std::cout
            << "failed to detect error in construction "
            << tresult_name << "<-" << targ_name
            << std::endl;
        boost::safe_numerics::checked::cast<TResult>(v);
        return false;
    }
    if(expected_result == '.' && r2.exception()){
        std::cout
            << "erroneously emitted error "
            << tresult_name << "<-" << targ_name
            << std::endl;
        boost::safe_numerics::checked::cast<TResult>(v);
        return false;
    }
    return true; // passed test
}

#include <boost/mp11/algorithm.hpp>
#include <boost/core/demangle.hpp>

using namespace boost::mp11;

// given a list of integral constants I, return a list of values
template<typename I>
struct get_values {
    static_assert(mp_is_list<I>(), "must be a list of two types");
    static_assert(2 == mp_size<I>::value, "must be a list of two types");
    constexpr static const size_t first = mp_first<I>(); // index of first argument
    constexpr static const size_t second = mp_second<I>();// index of second argument
};

struct test_pair {
    bool m_error;
    test_pair(bool b = true) : m_error(b) {}
    operator bool(){
        return m_error;
    }
    template<typename I>
    void operator()(const I &){
        using pair = get_values<I>;
        using TResult = mp_at<test_types, mp_first<I>>;
        using TArg = typename mp_at<test_values, mp_second<I>>::value_type;
        constexpr static const TArg v = mp_at<test_values, mp_second<I>>()();
        m_error &= test_cast<TResult>(
            v,
            boost::core::demangle(typeid(TResult).name()).c_str(),
            boost::core::demangle(typeid(TArg).name()).c_str(),
            test_result_cast[pair::first][pair::second]
        );
    }
};

int main(){
    // list of indices for values (integral constants)
    using value_indices = mp_iota_c<mp_size<test_values>::value>;
    // list of indices for types (integral constants)
    using type_indices = mp_iota_c<mp_size<test_types>::value>;

    // test runtime behavior
    test_pair rval(true);
    mp_for_each<
        mp_product<mp_list, type_indices, value_indices>
    >(rval);

    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
