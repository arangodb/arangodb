
// Copyright 2019 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>

using boost::mp11::mp_int;

template<class N> using mod_2 = mp_int<N::value % 2>;
template<class N> using mod_3 = mp_int<N::value % 3>;
template<class N> using mod_6 = mp_int<N::value % 6>;

using boost::mp11::mp_not;
using boost::mp11::mp_plus;

template<class T> using P1 = mp_not<mod_6<T>>;
template<class T1, class... T> using P2 = mp_not<mp_plus<T...>>;

using boost::mp11::mp_bool;

template<std::size_t N> struct second_is
{
    template<class T1, class T2> using fn = mp_bool< T2::value == N >;
};

using boost::mp11::mp_first;
using boost::mp11::mp_filter_q;
using boost::mp11::mp_iota;
using boost::mp11::mp_size;

template<class L, std::size_t N> using at_c = mp_first< mp_filter_q< second_is<N>, L, mp_iota<mp_size<L>> > >;

int main()
{
    using boost::mp11::mp_iota_c;
    using boost::mp11::mp_filter;
    using boost::mp11::mp_list;
    using boost::mp11::mp_size_t;
    using boost::mp11::mp_transform;

    {
        int const N = 12;
        using L1 = mp_iota_c<N>;

        using R1 = mp_filter<P1, L1>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R1, mp_list<mp_size_t<0>, mp_size_t<6>>>));

        using L2 = mp_transform<mod_2, L1>;
        using L3 = mp_transform<mod_3, L1>;
        using L6 = mp_transform<mod_6, L1>;

        using R2 = mp_filter<P2, L1, L6>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R2, mp_list<mp_size_t<0>, mp_size_t<6>>>));

        using R3 = mp_filter<P2, L1, L2, L3>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R3, mp_list<mp_size_t<0>, mp_size_t<6>>>));
    }

    {
        int const N = 64;
        int const M = 17;

        using L1 = mp_iota_c<N>;
        using R1 = at_c<L1, M>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<R1, mp_size_t<M>>));
    }

    return boost::report_errors();
}
