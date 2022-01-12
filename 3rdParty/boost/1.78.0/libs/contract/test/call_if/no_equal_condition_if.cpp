
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test assertions skipped when operations to check them missing (e.g., `==`).

// C++17 warning from Boost.Bind.
#define _SILENCE_CXX17_ADAPTOR_TYPEDEFS_DEPRECATION_WARNING

#include <boost/contract/function.hpp>
#include <boost/contract/check.hpp>
#include <boost/contract/assert.hpp>
#include <boost/contract/call_if.hpp>
#include <boost/bind.hpp>
#include <boost/type_traits/has_equal_to.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <functional>
#include <vector>

unsigned equal_skips;

template<typename T>
void push_back(std::vector<T>& vect, T const& value) {
    boost::contract::check c = boost::contract::function()
        .postcondition([&] {
            BOOST_CONTRACT_ASSERT(
                boost::contract::condition_if<boost::has_equal_to<T> >(
                    boost::bind(std::equal_to<T>(), boost::cref(vect.back()),
                            boost::cref(value))
                )
            );
            if(!boost::has_equal_to<T>::value) ++equal_skips;
        })
    ;
    vect.push_back(value);
}

struct j { // Type without operator==.
    explicit j(int /* i */) {}
};

int main() {
    std::vector<int> vi;
    equal_skips = 0;
    push_back(vi, 123);
    BOOST_TEST_EQ(equal_skips, 0u);
        
    unsigned const cnt =
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            1
        #else
            0
        #endif
    ;

    j jj(456);
    std::vector<j> vj;
    equal_skips = 0;
    push_back(vj, jj);
    BOOST_TEST_EQ(equal_skips, cnt);

    return boost::report_errors();
}

