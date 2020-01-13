//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include <boost/safe_numerics/checked_result.hpp>
#include <boost/safe_numerics/checked_result_operations.hpp>

int main(){
    using ext_uint = boost::safe_numerics::checked_result<unsigned int>;
    const ext_uint x{4};
    const ext_uint y{3};

    // operation is a success!
    std::cout << "success! x - y = " << x - y;

    // subtraction would result in -1, and invalid result for an unsigned value
    std::cout << "problem: y - x = " << y - x;

    const ext_uint z = y - x;
    std::cout << "z = " << z;
    // sum of two negative overflows is a negative overflow.
    std::cout << "z + z" << z + z;

    return 0;
}
