// Copyright (c) 2016-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include "boost/pfr.hpp"

struct my_struct { // no ostream operator defined!
    std::string s;
    int i;
};

int main() {
    my_struct s{{"Das ist fantastisch!"}, 100};

    std::cout << "my_struct has "
        << boost::pfr::tuple_size<my_struct>::value // Outputs: 2
        << " fields: "
        << boost::pfr::io(s); // Outputs: {"Das ist fantastisch!", 100};
}
