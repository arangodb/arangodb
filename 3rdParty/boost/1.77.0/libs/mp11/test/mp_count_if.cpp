
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/detail/config.hpp>

#if BOOST_MP11_MSVC
# pragma warning( disable: 4503 ) // decorated name length exceeded
#endif

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

struct X1 {};

using boost::mp11::mp_bool;

template<class T> using is_even = mp_bool< T::value % 2 == 0 >;

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_count_if;
    using boost::mp11::mp_size_t;

    {
        using L1 = mp_list<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count_if<L1, std::is_const>, mp_size_t<0>>));

        using L2 = mp_list<X1, X1 const, X1*, X1 const, X1*, X1*>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count_if<L2, std::is_volatile>, mp_size_t<0>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count_if<L2, std::is_const>, mp_size_t<2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count_if<L2, std::is_pointer>, mp_size_t<3>>));
    }

    {
        using L1 = std::tuple<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count_if<L1, std::is_const>, mp_size_t<0>>));

        using L2 = std::tuple<X1, X1 const, X1*, X1 const, X1*, X1*>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count_if<L2, std::is_volatile>, mp_size_t<0>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count_if<L2, std::is_const>, mp_size_t<2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count_if<L2, std::is_pointer>, mp_size_t<3>>));
    }

    {
        using L2 = std::pair<X1 const, X1*>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count_if<L2, std::is_volatile>, mp_size_t<0>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count_if<L2, std::is_const>, mp_size_t<1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_count_if<L2, std::is_pointer>, mp_size_t<1>>));
    }

    {
        using boost::mp11::mp_iota_c;

        int const N = 1089;

        using L = mp_iota_c<N>;
        using R = mp_count_if<L, is_even>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<R, mp_size_t<(N + 1) / 2>>));
    }

    return boost::report_errors();
}
