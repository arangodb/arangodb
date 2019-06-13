
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>

int gcd(int const a, int const b) {
    int result;
    boost::contract::check c = boost::contract::function()
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(a > 0);
            BOOST_CONTRACT_ASSERT(b > 0);
        })
        .postcondition([&] {
            BOOST_CONTRACT_ASSERT(result <= a);
            BOOST_CONTRACT_ASSERT(result <= b);
        })
    ;

    int x = a, y = b;
    while(x != y) {
        if(x > y) x = x - y;
        else y = y - x;
    }
    return result = x;
}

//[check_macro
int main() {
    // Implementation checks (via macro, disable run-/compile-time overhead).
    BOOST_CONTRACT_CHECK(gcd(12, 28) == 4);
    BOOST_CONTRACT_CHECK(gcd(4, 14) == 2);

    return 0;
}
//]

