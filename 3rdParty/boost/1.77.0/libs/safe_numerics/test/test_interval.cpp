//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <limits>
#include <functional>
#include <array>

#include <boost/core/demangle.hpp>
#include <boost/safe_numerics/checked_result.hpp>
#include <boost/safe_numerics/checked_result_operations.hpp>
#include <boost/safe_numerics/interval.hpp>

template<typename T>
using fptr = T (*)(const T &, const T &);
template<typename T>
using fptr_interval = fptr<boost::safe_numerics::interval<T>>;

template<typename T>
struct op {
    const fptr<T> m_f;
    const fptr_interval<T> m_finterval;
    const char * m_symbol;
    const bool skip_zeros;
};

template<
    typename T,
    unsigned int N
>
bool test_type_operator(
    const T (&value)[N],
    const op<T> & opi
){
    using namespace boost::safe_numerics;

    // for each pair of values p1, p2 (100)
    for(const T & l1 : value)
    for(const T & u1 : value){
        if(l1 > u1) continue; // skip reverse range
        const interval<T> p1(l1, u1);
        for(const T & l2 : value)
        for(const T & u2 : value){
            if(l2 > u2) continue; // skip reverse range
            const interval<T> p2(l2, u2);

            // maybe skip intervals which include zero
            if(opi.skip_zeros){
                if(l2 == safe_numerics_error::range_error
                || l2 == safe_numerics_error::domain_error
                || u2 == safe_numerics_error::range_error
                || u2 == safe_numerics_error::domain_error
                || p2.includes(T(0))
                )
                    continue;
            }

            // create a new interval from the operation
            const interval<T> result_interval = opi.m_finterval(p1, p2);
            std::cout
                << p1 << opi.m_symbol << p2 << " -> " << result_interval << std::endl;
            
            // if resulting interval is null
            if(result_interval.u < result_interval.l)
                continue;

            // for each pair test values
            for(const T r1 : value)
            for(const T r2 : value){
                // calculate result of operation
                const T result = opi.m_f(r1, r2);
                if(result != safe_numerics_error::range_error
                && result != safe_numerics_error::domain_error ){
                    // note usage of tribool logic here !!!
                    // includes returns indeterminate the conditional
                    // returns false in both cases and this is what we want.
                    // This is very subtle, don't skim over this.
                    // if both r1 and r2 are within they're respective bounds
                    if(p1.includes(r1) && p2.includes(r2)
                    && ! result_interval.includes(result)){
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

// values
// note: need to explicitly specify number of elements to avoid msvc failure
template<typename T>
const boost::safe_numerics::checked_result<T> value[8] = {
    boost::safe_numerics::safe_numerics_error::negative_overflow_error,
    std::numeric_limits<T>::lowest(),
    T(-1),
    T(0),
    T(1),
    std::numeric_limits<T>::max(),
    boost::safe_numerics::safe_numerics_error::positive_overflow_error,
    boost::safe_numerics::safe_numerics_error::domain_error
};

// note: need to explicitly specify number of elements to avoid msvc failure
template<typename T>
const boost::safe_numerics::checked_result<T> unsigned_value[6] = {
    boost::safe_numerics::safe_numerics_error::negative_overflow_error,
    T(0),
    T(1),
    std::numeric_limits<T>::max(),
    boost::safe_numerics::safe_numerics_error::positive_overflow_error,
    boost::safe_numerics::safe_numerics_error::domain_error
};

// invoke for each type
struct test_type {
    unsigned int m_error_count;
    test_type() :
        m_error_count(0)
    {}
    template<typename T>
    bool operator()(const T &){
        using namespace boost::safe_numerics;
        std::cout
            << "** testing "
            << boost::core::demangle(typeid(T).name())
            << std::endl;

        using R = checked_result<T>;
        // pointers to operands for types T
        static const std::array<op<R>, 5> op_table{{
            {operator+, operator+, "+", false},
            {operator-, operator-, "-", false},
            {operator*, operator*, "*", false},
            {operator<<, operator<<, "<<", false},
            {operator>>, operator>>, ">>", false},
        }};

        //for(unsigned int i = 0; i < sizeof(op_table)/sizeof(op) / sizeof(fptr<R>); ++i){
        for(const op<R> & o : op_table){
            if(std::is_signed<T>::value){
                if(! test_type_operator(value<T>, o)){
                    ++m_error_count;
                    return false;
                }
            }
            else{
                if(! test_type_operator(unsigned_value<T>, o)){
                    ++m_error_count;
                    return false;
                }
            }
        }
        return true;
    }
};

#include <boost/mp11/list.hpp>
#include <boost/mp11/algorithm.hpp>

int main(int, char *[]){
    using namespace boost::mp11;
    // list of signed types
    using signed_types = mp_list<std::int8_t, std::int16_t, std::int32_t, std::int64_t>;
    // list of unsigned types
    using unsigned_types = mp_list<std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t>;

    test_type t;
    mp_for_each<unsigned_types>(t);
    mp_for_each<signed_types>(t);

    std::cout << (t.m_error_count == 0 ? "success!" : "failure") << std::endl;
    return t.m_error_count ;
}
