// Copyright 2020 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <cstddef>

struct X1 {};
struct X2 { using first_type = double; using next_type = X1; };
struct X3 { using first_type = float; using next_type = X2; };
struct X4 { using first_type = int; using next_type = X3; };

template<class T> using first_type = typename T::first_type;
template<class T> using next_type = typename T::next_type;

template<class T1, class T2> struct cons {};

template<class T1, class T2> struct cons2
{
    using first_type = T1;
    using next_type = T2;
};

using boost::mp11::mp_reverse_fold;
using boost::mp11::mp_iterate;
using boost::mp11::mp_first;
using boost::mp11::mp_second;
using boost::mp11::mp_rest;

template<class L1> void test()
{
    using R1 = mp_iterate<L1, mp_first, mp_rest>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<L1, R1>));

    using V2 = mp_reverse_fold<L1, void, cons>;
    using R2 = mp_iterate<V2, mp_first, mp_second>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<L1, R2>));

#if !BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, < 1900 )

    using V3 = mp_reverse_fold<L1, void, cons2>;
    using R3 = mp_iterate<V3, first_type, next_type>;
    BOOST_TEST_TRAIT_TRUE((std::is_same<L1, R3>));

#endif
}

int main()
{
    using boost::mp11::mp_list;

    test< mp_list<> >();
    test< mp_list<int> >();
    test< mp_list<int, void> >();
    test< mp_list<int, void, float> >();
    test< mp_list<int, void, float, double> >();

    using boost::mp11::mp_identity_t;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_iterate<X4, mp_identity_t, next_type>, mp_list<X4, X3, X2, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_iterate<X4, first_type, next_type>, mp_list<int, float, double>>));

    return boost::report_errors();
}
