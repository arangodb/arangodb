// protect_test2.cpp
//
// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/bind/protect.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/core/is_same.hpp>

template<class T, class F> void test( F )
{
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<typename T::result_type, typename F::result_type>));
}

struct X
{
    struct result_type {};
};

struct Y
{
};

template<class T, class U> struct inherit: T, U
{
};

template<class F> void test2( F )
{
    // test that F doesn't have ::result_type
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<typename inherit<F, X>::result_type, typename X::result_type>));
}

int main()
{
    test<X>( boost::protect( X() ) );

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && !defined(BOOST_NO_CXX11_DECLTYPE)

    test2( boost::protect( Y() ) );

#endif

    return boost::report_errors();
}
