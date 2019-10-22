
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test auto error after post (for free func, but same for all contracts).

#include <boost/contract/function.hpp>

int main() {
    auto c = boost::contract::function() // Error (can't use auto).
        .precondition([] {})
        .old([] {})
        .postcondition([] {})
    ;
    return 0;
}

