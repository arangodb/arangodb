
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include <boost/contract.hpp>
#include <boost/optional.hpp>
#include <vector>
#include <cassert>

//[optional_result
template<unsigned Index, typename T>
T& get(std::vector<T>& vect) {
    boost::optional<T&> result; // Result not initialized here...
    boost::contract::check c = boost::contract::function()
        .precondition([&] {
            BOOST_CONTRACT_ASSERT(Index < vect.size());
        })
        .postcondition([&] {
            BOOST_CONTRACT_ASSERT(*result == vect[Index]);
        })
    ;
    
    // Function body (executed after preconditions checked).
    return *(result = vect[Index]); // ...result initialized here instead.
}
//]

int main() {
    std::vector<int> v;
    v.push_back(123);
    v.push_back(456);
    v.push_back(789);
    int& x = get<1>(v);
    assert(x == 456);
    x = -456;
    assert(v[1] == -456);
    return 0;
}

