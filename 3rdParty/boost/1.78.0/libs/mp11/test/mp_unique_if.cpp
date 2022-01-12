// Copyright 2015 Peter Dimov.
// Copyright 2019 Kris Jusiak.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/detail/config.hpp>

#if BOOST_MP11_WORKAROUND( BOOST_MP11_GCC, < 40800 )
# pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <cstddef>

using boost::mp11::mp_bool;

#if !BOOST_MP11_WORKAROUND( BOOST_MSVC, < 1910 )

template<class T, class U> using Eq1 = mp_bool< T::value == U::value >;
template<class T, class U> using Eq2 = mp_bool< sizeof(T) == sizeof(U) >;

#else

template<class T, class U> struct Eq1: mp_bool< T::value == U::value > {};
template<class T, class U> struct Eq2: mp_bool< sizeof(T) == sizeof(U) > {};

#endif

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_unique_if;

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<mp_list<>, std::is_same>, mp_list<>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<mp_list<void>, std::is_same>, mp_list<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<mp_list<void, void>, std::is_same>, mp_list<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<mp_list<void, void, void>, std::is_same>, mp_list<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<mp_list<void, void, void, void>, std::is_same>, mp_list<void>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<mp_list<void, int>, std::is_same>, mp_list<void, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<mp_list<void, void, void, int, int, int>, std::is_same>, mp_list<void, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<mp_list<void, int, void, int, int, void, int, int, int>, std::is_same>, mp_list<void, int>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<mp_list<void, int, char[]>, std::is_same>, mp_list<void, int, char[]>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<mp_list<void, int, char[], void, int, char[], void, int, char[]>, std::is_same>, mp_list<void, int, char[]>>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<std::tuple<>, std::is_same>, std::tuple<>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<std::tuple<void>, std::is_same>, std::tuple<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<std::tuple<void, void>, std::is_same>, std::tuple<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<std::tuple<void, void, void>, std::is_same>, std::tuple<void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<std::tuple<void, void, void, void>, std::is_same>, std::tuple<void>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<std::tuple<void, int>, std::is_same>, std::tuple<void, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<std::tuple<void, void, void, int, int, int>, std::is_same>, std::tuple<void, int>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<std::tuple<void, int, void, int, int, void, int, int, int>, std::is_same>, std::tuple<void, int>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<std::tuple<void, int, char[]>, std::is_same>, std::tuple<void, int, char[]>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<std::tuple<void, int, char[], void, int, char[], void, int, char[]>, std::is_same>, std::tuple<void, int, char[]>>));
    }

    {
        using boost::mp11::mp_iota_c;
        using boost::mp11::mp_size_t;

        using L1 = mp_iota_c<32>;

        using R1 = mp_unique_if<L1, std::is_same>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R1, L1>));

        using R2 = mp_unique_if<L1, Eq1>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R2, L1>));

        using R3 = mp_unique_if<L1, Eq2>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R3, mp_list<mp_size_t<0>>>));
    }

    {
        using boost::mp11::mp_list_c;
        using boost::mp11::mp_append;
        using boost::mp11::mp_push_back;
        using boost::mp11::mp_int;

        using L1 = mp_list_c<std::size_t, 0, 1, 2, 3>;
        using L2 = mp_list_c<int, 1, 2, 3, 4>;
        using L3 = mp_list_c<long, 2, 3, 4, 5>;

        using R1 = mp_unique_if<mp_append<L1, L2, L3>, Eq1>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R1, mp_push_back<L1, mp_int<4>, std::integral_constant<long, 5>>>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<std::tuple<char[1], char[2], char[1], char[2], char[2], char[1], char[2], char[2], char[2]>, Eq2>, std::tuple<char[1], char[2]>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if<std::tuple<char[1], char[2], char[3], char[1], char[2], char[3], char[1], char[2], char[3]>, Eq2>, std::tuple<char[1], char[2], char[3]>>));
    }

    return boost::report_errors();
}
