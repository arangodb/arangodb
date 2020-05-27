
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[n1962_factorial
#include <boost/contract.hpp>
#include <cassert>

int factorial(int n) {
    int result;
    boost::contract::check c = boost::contract::function()
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(n >= 0); // Non-negative natural number.
            BOOST_CONTRACT_ASSERT(n <= 12); // Max function input.
        })
        .postcondition([&] {
            BOOST_CONTRACT_ASSERT(result >= 1);
            if(n < 2) { // Select assertion.
                BOOST_CONTRACT_ASSERT(result == 1);
            } else {
                // Assertions automatically disabled in other assertions.
                // Therefore, this postcondition can recursively call the
                // function without causing infinite recursion.
                BOOST_CONTRACT_ASSERT_AUDIT(n * factorial(n - 1));
            }
        })
    ;

    return n < 2 ? (result = 1) : (result = n * factorial(n - 1));
}

int main() {
    assert(factorial(4) == 24);
    return 0;
}
//]

