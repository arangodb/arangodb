
// Copyright 2015-2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/function.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

template<class T, T N> using integral = std::integral_constant<T, N>;

int main()
{
    using boost::mp11::mp_plus;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_plus<>, integral<int, 0>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_plus<integral<char, 1>>, integral<int, 1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_plus<integral<short, 2>>, integral<int, 2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_plus<integral<int, 3>>, integral<int, 3>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_plus<integral<unsigned, 4>>, integral<unsigned, 4>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_plus<integral<char, 1>, integral<char, 2>>, integral<int, 3>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_plus<integral<short, 1>, integral<short, 2>, integral<short, 3>>, integral<int, 6>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_plus<integral<int, 1>, integral<int, 2>, integral<int, 3>, integral<long, 4>>, integral<long, 10>>));

    return boost::report_errors();
}
