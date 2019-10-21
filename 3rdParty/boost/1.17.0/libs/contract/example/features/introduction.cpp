
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <limits>
#include <cassert>

//[introduction
#include <boost/contract.hpp>

void inc(int& x) {
    boost::contract::old_ptr<int> old_x = BOOST_CONTRACT_OLDOF(x); // Old value.
    boost::contract::check c = boost::contract::function()
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(x < std::numeric_limits<int>::max()); // Line 17.
        })
        .postcondition([&] {
            BOOST_CONTRACT_ASSERT(x == *old_x + 1); // Line 20.
        })
    ;

    ++x; // Function body.
}
//]

int main() {
    int x = 10;
    inc(x);
    assert(x == 11);
    return 0;
}

