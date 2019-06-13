
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[n1962_sqrt
#include <boost/contract.hpp>
#include <cmath>
#include <cassert>

long lsqrt(long x) {
    long result;
    boost::contract::check c = boost::contract::function()
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(x >= 0);
        })
        .postcondition([&] {
            BOOST_CONTRACT_ASSERT(result * result <= x);
            BOOST_CONTRACT_ASSERT((result + 1) * (result + 1) > x);
        })
    ;

    return result = long(std::sqrt(double(x)));
}

int main() {
    assert(lsqrt(4) == 2);
    return 0;
}
//]

