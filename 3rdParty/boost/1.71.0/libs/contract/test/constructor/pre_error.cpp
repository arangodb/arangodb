
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test constructor cannot use `.precondition(...)`.

#include <boost/contract/constructor.hpp>
#include <boost/contract/check.hpp>

struct a {
    a() {
        boost::contract::check c = boost::contract::constructor(this)
            .precondition([] {}) // Error (must use constructor_precondition).
        ;
    }
};

int main() {
    a aa;
    return 0;
}

