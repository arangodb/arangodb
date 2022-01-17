
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[n1962_sum
#include <boost/contract.hpp>
#include <cassert>

int sum(int count, int* array) {
    int result;
    boost::contract::check c = boost::contract::function()
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(count % 4 == 0);
        })
    ;

    result = 0;
    for(int i = 0; i < count; ++i) result += array[i];
    return result;
}

int main() {
    int a[4] = {1, 2, 3, 4};
    assert(sum(4, a) == 10);
    return 0;
}
//]

