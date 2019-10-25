
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test that OLD macro allows to use C++11 auto declarations.

#include <boost/config.hpp>
#include <boost/contract/old.hpp>
#include <boost/type_traits.hpp>
#include <boost/static_assert.hpp>
#include <boost/detail/lightweight_test.hpp>

int main() {
    int x = -123;
    auto old_x = BOOST_CONTRACT_OLDOF(x);
    x = 123;
    BOOST_STATIC_ASSERT((boost::is_same<decltype(old_x),
            boost::contract::old_ptr<int> >::value));
    #ifndef BOOST_CONTRACT_NO_OLDS
        BOOST_TEST_EQ(*old_x, -123);
    #endif
    BOOST_TEST_EQ(x, 123);

    boost::contract::virtual_* v = 0;
    char y = 'j';
    auto old_y = BOOST_CONTRACT_OLDOF(v, y);
    y = 'k';
    BOOST_STATIC_ASSERT((boost::is_same<decltype(old_y),
            boost::contract::old_ptr<char> >::value));
    #ifndef BOOST_CONTRACT_NO_OLDS
        BOOST_TEST_EQ(*old_y, 'j');
    #endif
    BOOST_TEST_EQ(y, 'k');
    return boost::report_errors();
}

