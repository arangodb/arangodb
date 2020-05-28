#ifndef BOOST_SAFE_NUMERICS_TEST_CHECKED_VALUES_HPP
#define BOOST_SAFE_NUMERICS_TEST_CHECKED_VALUES_HPP

//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/safe_numerics/checked_result.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/algorithm.hpp>

// values
// note: In theory explicity specifying the number of elements in the
// array should not be necessary.  However, this seems to crash the CLang
// compilers with standards setting of C++17.  So don't remove the array
// bounds below
template<typename T>
constexpr const boost::safe_numerics::checked_result<T> signed_values[9] = {
    boost::safe_numerics::safe_numerics_error::range_error,
    boost::safe_numerics::safe_numerics_error::domain_error,
    boost::safe_numerics::safe_numerics_error::positive_overflow_error,
    std::numeric_limits<T>::max(),
    1,
    0,
    -1,
    std::numeric_limits<T>::lowest(),
    boost::safe_numerics::safe_numerics_error::negative_overflow_error,
};

using signed_test_types = boost::mp11::mp_list<
    std::int8_t, std::int16_t, std::int32_t, std::int64_t
>;
using signed_value_indices = boost::mp11::mp_iota_c<
    sizeof(signed_values<int>) / sizeof(signed_values<int>[0])
>;

template<typename T>
constexpr const boost::safe_numerics::checked_result<T> unsigned_values[7] = {
    boost::safe_numerics::safe_numerics_error::range_error,
    boost::safe_numerics::safe_numerics_error::domain_error,
    boost::safe_numerics::safe_numerics_error::positive_overflow_error,
    std::numeric_limits<T>::max(),
    1,
    0,
    boost::safe_numerics::safe_numerics_error::negative_overflow_error,
};

using unsigned_test_types = boost::mp11::mp_list<
    std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
>;
using unsigned_value_indices = boost::mp11::mp_iota_c<
    sizeof(unsigned_values<int>) / sizeof(unsigned_values<int>[0])
>;

#endif // BOOST_SAFE_NUMERICS_TEST_CHECKED_VALUES_HPP
