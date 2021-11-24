
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test forcing compiler error for old values of non-copyable types.

#include "if_copyable.hpp"
#include <boost/contract/function.hpp>
#include <boost/contract/old.hpp>
#include <boost/contract/check.hpp>
#include <boost/contract/assert.hpp>
#include <boost/noncopyable.hpp>


template<typename T>
void next(T& x) {
    boost::contract::old_ptr<T> old_x = BOOST_CONTRACT_OLDOF(x);
    boost::contract::check c = boost::contract::function()
        .postcondition([&] {
            // No need to check `if(old_x) ...` here.
            BOOST_CONTRACT_ASSERT(x == *old_x + T(1));
            #ifdef BOOST_CONTRACT_NO_ALL
                #error "force error if no contracts (ASSERT expands to nothing)"
            #endif
        })
    ;
    ++x;
}

int main() {
    int i = 1; // Test built-in copyable type.
    cp c(1); // Test custom copyable type.
    ncp n(1); // Test non-copyable type.

    next(i); // OK.
    next(c); // OK.
    next(n); // Error.

    return 0;
}

