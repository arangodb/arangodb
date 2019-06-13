
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <limits>
#include <cassert>

//[non_member
#include <boost/contract.hpp>

// Contract for a non-member function.
int inc(int& x) {
    int result;
    boost::contract::old_ptr<int> old_x = BOOST_CONTRACT_OLDOF(x);
    boost::contract::check c = boost::contract::function()
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(x < std::numeric_limits<int>::max());
        })
        .postcondition([&] {
            BOOST_CONTRACT_ASSERT(x == *old_x + 1);
            BOOST_CONTRACT_ASSERT(result == *old_x);
        })
        .except([&] {
            BOOST_CONTRACT_ASSERT(x == *old_x);
        })
    ;

    return result = x++; // Function body.
}
//]

int main() {
    int x = std::numeric_limits<int>::max() - 1;
    assert(inc(x) == std::numeric_limits<int>::max() - 1);
    assert(x == std::numeric_limits<int>::max());
    return 0;
}

