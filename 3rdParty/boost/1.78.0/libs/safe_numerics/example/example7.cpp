//  Copyright (c) 2018 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cassert>
#include <stdexcept>
#include <sstream>
#include <iostream>

#include <boost/safe_numerics/safe_integer_range.hpp>

// NOT using safe numerics - enforce program contract explicitly
// return total number of minutes
unsigned int contract_convert(
    const unsigned int & hours,
    const unsigned int & minutes
) {
    // check that parameters are within required limits
    // invokes a runtime cost EVERYTIME the function is called
    // and the overhead of supporting an interrupt.
    // note high runtime cost!
    if(minutes > 59)
        throw std::domain_error("minutes exceeded 59");
    if(hours > 23)
        throw std::domain_error("hours exceeded 23");
    return hours * 60 + minutes;
}

// Use safe numerics to enforce program contract automatically
// define convenient typenames for hours and minutes hh:mm
using hours_t = boost::safe_numerics::safe_unsigned_range<0, 23>;
using minutes_t = boost::safe_numerics::safe_unsigned_range<0, 59>;
using minutes_total_t = boost::safe_numerics::safe_unsigned_range<0, 59>;

// return total number of minutes
// type returned is safe_unsigned_range<0, 24*60 - 1>
auto convert(const hours_t & hours, const minutes_t & minutes) {
    // no need to test pre-conditions
    // input parameters are guaranteed to hold legitimate values
    // no need to test post-conditions
    // return value guaranteed to hold result
    return hours * 60 + minutes;
}

unsigned int test1(unsigned int hours, unsigned int minutes){
    // problem: checking of externally produced value can be expensive
    // invalid parameters - detected - but at a heavy cost
    return contract_convert(hours, minutes);
}

auto test2(unsigned int hours, unsigned int minutes){
    // solution: use safe numerics
    // safe types can be implicitly constructed base types
    // construction guarentees corectness
    // return value is known to fit in unsigned int
    return convert(hours, minutes);
}

auto test3(unsigned int hours, unsigned int minutes){
    // actually we don't even need the convert function any more
    return hours_t(hours) * 60 + minutes_t(minutes);
}

int main(int, const char *[]){
    std::cout << "example 7: ";
    std::cout << "enforce contracts with zero runtime cost" << std::endl;

    unsigned int total_minutes;

    try {
        total_minutes = test3(17, 83);
        std::cout << "total minutes = " << total_minutes << std::endl;
    }
    catch(const std::exception & e){
        std::cout << "parameter error detected" << std::endl;
    }

    try {
        total_minutes = test3(17, 10);
        std::cout << "total minutes = " << total_minutes << std::endl;
    }
    catch(const std::exception & e){
        // should never arrive here
        std::cout << "parameter error erroneously detected" << std::endl;
        return 1;
    }
    return 0;
}
