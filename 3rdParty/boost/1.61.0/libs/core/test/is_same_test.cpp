//
// Test for core::is_same<T1,T2>
//
// Copyright 2014 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/core/is_same.hpp>
#include <boost/core/lightweight_test_trait.hpp>

struct X
{
};

struct Y
{
};

int main()
{
    BOOST_TEST_TRAIT_TRUE(( boost::core::is_same<X, X> ));
    BOOST_TEST_TRAIT_TRUE(( boost::core::is_same<Y, Y> ));
    BOOST_TEST_TRAIT_TRUE(( boost::core::is_same<void, void> ));
    BOOST_TEST_TRAIT_TRUE(( boost::core::is_same<int, int> ));
    BOOST_TEST_TRAIT_TRUE(( boost::core::is_same<void const volatile, void const volatile> ));

    BOOST_TEST_TRAIT_FALSE(( boost::core::is_same<X, Y> ));
    BOOST_TEST_TRAIT_FALSE(( boost::core::is_same<X, X const> ));
    BOOST_TEST_TRAIT_FALSE(( boost::core::is_same<X, void> ));
    BOOST_TEST_TRAIT_FALSE(( boost::core::is_same<X, int> ));
    BOOST_TEST_TRAIT_FALSE(( boost::core::is_same<int, void> ));
    BOOST_TEST_TRAIT_FALSE(( boost::core::is_same<void, void const volatile> ));

    return boost::report_errors();
}
