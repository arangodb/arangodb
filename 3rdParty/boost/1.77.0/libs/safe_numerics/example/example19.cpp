//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <exception>
#include <iostream>
#include <type_traits>
#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/safe_integer_range.hpp>
#include <boost/safe_numerics/safe_integer_literal.hpp>

#include <boost/safe_numerics/utility.hpp>

using namespace boost::safe_numerics;

int main(){
    safe_unsigned_range<7, 24> i;
    
    // since the range is included in [0,255], the underlying type of i
    // will be an unsigned char.
    try{
        i = 0;  // throws out_of_range exception
        std::cout << "fails to detect erroneous assignment" << std::endl;
    }
    catch(std::exception & e){
        // should arrive here
    }
    try{
        i = 9;  // ok - no exception expected
    }
    catch(std::exception & e){
        std::cout << "erroneous error for legal assignment" << std::endl;
    }
    try{
        i *= 9; // fails to compile because result can't fin in range
        std::cout << "fails to out of range result" << std::endl;
    }
    catch(std::exception & e){
        // should arrive here
    }
    try{
        i = -1; // throws out_of_range exception
        std::cout << "fails to detect erroneous assignment" << std::endl;
    }
    catch(std::exception & e){
        // should arrive here
    }
    std::uint8_t j = 4;
    auto k = i + j;

    // if either or both types are safe types, the result is a safe type
    // determined by promotion policy.  In this instance
    // the range of i is [7, 24] and the range of j is [0,255].
    // so the type of k will be a safe type with a range of [7,279]
    static_assert(
        is_safe<decltype(k)>::value
        && std::numeric_limits<decltype(k)>::min() == 7
        && std::numeric_limits<decltype(k)>::max() == 279,
        "k is a safe range of [7,279]"
    );
    return 0;
}
