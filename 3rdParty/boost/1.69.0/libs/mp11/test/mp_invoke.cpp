
//  Copyright 2015, 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/utility.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

using boost::mp11::mp_invoke;
using boost::mp11::mp_size_t;

struct Q1
{
    template<class...> using fn = void;
};

struct Q2
{
    template<class...> class fn;
};

struct Q3
{
    template<class... T> using fn = mp_size_t<sizeof...(T)>;
};

struct Q4
{
    template<class T1, class... T> using fn = T1;
};

struct Q5
{
    template<class T1, class T2> using fn = T2;
};

int main()
{
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q1>, void>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q1, int>, void>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q1, int[], char[]>, void>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q2>, Q2::fn<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q2, int>, Q2::fn<int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q2, int[], char[]>, Q2::fn<int[], char[]>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q3>, mp_size_t<0>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q3, int>, mp_size_t<1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q3, int[], char[]>, mp_size_t<2>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q4, int>, int>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q4, int[], char[]>, int[]>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q5, int, float>, float>));

    return boost::report_errors();
}
