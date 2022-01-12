//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/integer.hpp>
#include <boost/safe_numerics/utility.hpp>

// include headers to support safe integers
#include <boost/safe_numerics/cpp.hpp>

using promotion_policy = boost::safe_numerics::cpp<
    8,  // char      8 bits
    16, // short     16 bits
    16, // int       16 bits
    16, // long      32 bits
    32  // long long 32 bits
>;

template<typename R, typename T, typename U>
struct test {
    using ResultType = promotion_policy::result_type<T,U>;
    //boost::safe_numerics::utility::print_type<ResultType> pt;
    static_assert(
        std::is_same<R, ResultType>::value,
        "is_same<R, ResultType>"
    );
};

test<std::uint16_t, std::uint8_t, std::uint8_t> t1;

int main(){
    return 0;
}

