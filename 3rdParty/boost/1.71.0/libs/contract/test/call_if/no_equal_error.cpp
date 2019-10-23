
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test assertion error when operations to check them missing (e.g., `==`).

#include <boost/contract/function.hpp>
#include <boost/contract/check.hpp>
#include <boost/contract/assert.hpp>
#include <vector>

template<typename T>
void push_back(std::vector<T>& vect, T const& value) {
    boost::contract::check c = boost::contract::function()
        .postcondition([&] {
            BOOST_CONTRACT_ASSERT(vect.back() == value); // Error (j has no ==).
            #ifdef BOOST_CONTRACT_NO_ALL
                #error "force error if no contracts (ASSERT expands to nothing)"
            #endif
        })
    ;
    vect.push_back(value);
}

struct j { // Type without operator==.
    explicit j(int i) : j_(i) {}
private:
    int j_;
};

int main() {
    std::vector<int> vi;
    push_back(vi, 123);

    j jj(456);
    std::vector<j> vj;
    push_back(vj, jj);

    return 0;
}

