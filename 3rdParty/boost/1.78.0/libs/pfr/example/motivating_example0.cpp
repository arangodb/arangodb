// Copyright (c) 2016-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


//[pfr_motivating_example
#include <iostream>
#include <string>

#include "boost/pfr.hpp"

struct some_person {
    std::string name;
    unsigned birth_year;
};

int main() {
    some_person val{"Edgar Allan Poe", 1809};

    std::cout << boost::pfr::get<0>(val)                // No macro!
        << " was born in " << boost::pfr::get<1>(val);  // Works with any aggregate initializables!

    std::cout << boost::pfr::io(val);                   // Outputs: {"Edgar Allan Poe", 1809}
}
//]
