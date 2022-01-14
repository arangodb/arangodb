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
#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>

using boost::mp11::mp_bool;

#if !BOOST_MP11_WORKAROUND( BOOST_MSVC, < 1910 )

struct Q1
{
    template<class T, class U> using fn = mp_bool< T::value == U::value >;
};

struct Q2
{
    template<class T, class U> using fn = mp_bool< sizeof(T) == sizeof(U) >;
};

#else

struct Q1
{
    template<class T, class U> struct fn: mp_bool< T::value == U::value > {};
};

struct Q2
{
    template<class T, class U> struct fn: mp_bool< sizeof(T) == sizeof(U) > {};
};

#endif

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_unique_if_q;
    using boost::mp11::mp_quote_trait;

    {
        using boost::mp11::mp_iota_c;
        using boost::mp11::mp_size_t;

        using L1 = mp_iota_c<32>;

        using R1 = mp_unique_if_q<L1, mp_quote_trait<std::is_same>>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R1, L1>));

        using R2 = mp_unique_if_q<L1, Q1>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R2, L1>));

        using R3 = mp_unique_if_q<L1, Q2>;
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

        using R1 = mp_unique_if_q<mp_append<L1, L2, L3>, Q1>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R1, mp_push_back<L1, mp_int<4>, std::integral_constant<long, 5>>>));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if_q<std::tuple<char[1], char[2], char[1], char[2], char[2], char[1], char[2], char[2], char[2]>, Q2>, std::tuple<char[1], char[2]>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_unique_if_q<std::tuple<char[1], char[2], char[3], char[1], char[2], char[3], char[1], char[2], char[3]>, Q2>, std::tuple<char[1], char[2], char[3]>>));
    }

    return boost::report_errors();
}
