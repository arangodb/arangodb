//
// Test for lightweight_test_trait.hpp
//
// Copyright (c) 2014 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/core/lightweight_test_trait.hpp>

template<class T1, class T2> struct Y1
{
    enum { value = 1 };
};

template<class T1, class T2> struct Y2
{
    enum { value = 0 };
};

struct X1
{
    typedef int type;
};

struct X2
{
    typedef int type;
};

int main()
{
    // BOOST_TEST_TRAIT_TRUE

    BOOST_TEST_TRAIT_TRUE(( Y1<X1::type, X2::type> ));

    // BOOST_TEST_TRAIT_FALSE

    BOOST_TEST_TRAIT_FALSE(( Y2<X1::type, X2::type> ));

    return boost::report_errors();
}
