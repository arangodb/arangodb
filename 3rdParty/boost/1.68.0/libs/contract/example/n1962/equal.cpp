
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

//[n1962_equal
#include <boost/contract.hpp>
#include <cassert>

// Forward declaration because == and != contracts use one another's function.
template<typename T>
bool operator==(T const& left, T const& right);

template<typename T>
bool operator!=(T const& left, T const& right) {
    bool result;
    boost::contract::check c = boost::contract::function()
        .postcondition([&] {
            BOOST_CONTRACT_ASSERT(result == !(left == right));
        })
    ;

    return result = (left.value != right.value);
}

template<typename T>
bool operator==(T const& left, T const& right) {
    bool result;
    boost::contract::check c = boost::contract::function()
        .postcondition([&] {
            BOOST_CONTRACT_ASSERT(result == !(left != right));
        })
    ;

    return result = (left.value == right.value);
}

struct number { int value; };

int main() {
    number n;
    n.value = 123;

    assert((n == n) == true);   // Explicitly call operator==.
    assert((n != n) == false);  // Explicitly call operator!=.

    return 0;
}
//]

