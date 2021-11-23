// Copyright 2020 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/mp11/function.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <cstddef>

struct X1 {};
struct X2 {};
struct X3 {};

template<class T1, class T2> struct F {};

struct Q
{
    template<class T1, class T2> struct fn {};
};

int main()
{
    using boost::mp11::mp_partial_sum;
    using boost::mp11::mp_list;
    using boost::mp11::mp_list_c;
    using boost::mp11::mp_int;
    using boost::mp11::mp_size_t;
    using boost::mp11::mp_plus;
    using boost::mp11::mp_rename;
    using boost::mp11::mp_partial_sum_q;
    using boost::mp11::mp_quote;

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum< mp_list<>, void, F >, mp_list<> >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum< mp_list<X1>, void, F >, mp_list< F<void, X1> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum< mp_list<X1, X2>, void, F >, mp_list< F<void, X1>, F<F<void, X1>, X2> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum< mp_list<X1, X2, X3>, void, F >, mp_list< F<void, X1>, F<F<void, X1>, X2>, F<F<F<void, X1>, X2>, X3> > >));

        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum< mp_list_c<int, 7, 7, 2>, mp_int<0>, mp_plus >, mp_list< mp_int<7>, mp_int<14>, mp_int<16> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum< mp_list_c<std::size_t, 7, 7, 2>, mp_size_t<0>, mp_plus >, mp_list< mp_size_t<7>, mp_size_t<14>, mp_size_t<16> > >));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< mp_list<>, void, mp_quote<F> >, mp_list<> >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< mp_list<X1>, void, mp_quote<F> >, mp_list< F<void, X1> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< mp_list<X1, X2>, void, mp_quote<F> >, mp_list< F<void, X1>, F<F<void, X1>, X2> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< mp_list<X1, X2, X3>, void, mp_quote<F> >, mp_list< F<void, X1>, F<F<void, X1>, X2>, F<F<F<void, X1>, X2>, X3> > >));

        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< mp_list<>, void, Q >, mp_list<> >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< mp_list<X1>, void, Q >, mp_list< Q::fn<void, X1> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< mp_list<X1, X2>, void, Q >, mp_list< Q::fn<void, X1>, Q::fn<Q::fn<void, X1>, X2> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< mp_list<X1, X2, X3>, void, Q >, mp_list< Q::fn<void, X1>, Q::fn<Q::fn<void, X1>, X2>, Q::fn<Q::fn<Q::fn<void, X1>, X2>, X3> > >));

        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< mp_list_c<int, 7, 7, 2>, mp_int<0>, mp_quote<mp_plus> >, mp_list< mp_int<7>, mp_int<14>, mp_int<16> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< mp_list_c<std::size_t, 7, 7, 2>, mp_size_t<0>, mp_quote<mp_plus> >, mp_list< mp_size_t<7>, mp_size_t<14>, mp_size_t<16> > >));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum< std::tuple<>, void, F >, std::tuple<> >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum< std::tuple<X1>, void, F >, std::tuple< F<void, X1> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum< std::tuple<X1, X2>, void, F >, std::tuple< F<void, X1>, F<F<void, X1>, X2> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum< std::tuple<X1, X2, X3>, void, F >, std::tuple< F<void, X1>, F<F<void, X1>, X2>, F<F<F<void, X1>, X2>, X3> > >));

        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum< mp_rename<mp_list_c<int, 7, 7, 2>, std::tuple>, mp_int<0>, mp_plus >, std::tuple< mp_int<7>, mp_int<14>, mp_int<16> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum< mp_rename<mp_list_c<std::size_t, 7, 7, 2>, std::tuple>, mp_size_t<0>, mp_plus >, std::tuple< mp_size_t<7>, mp_size_t<14>, mp_size_t<16> > >));
    }

    {
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< std::tuple<>, void, mp_quote<F> >, std::tuple<> >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< std::tuple<X1>, void, mp_quote<F> >, std::tuple< F<void, X1> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< std::tuple<X1, X2>, void, mp_quote<F> >, std::tuple< F<void, X1>, F<F<void, X1>, X2> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< std::tuple<X1, X2, X3>, void, mp_quote<F> >, std::tuple< F<void, X1>, F<F<void, X1>, X2>, F<F<F<void, X1>, X2>, X3> > >));

        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< std::tuple<>, void, Q >, std::tuple<> >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< std::tuple<X1>, void, Q >, std::tuple< Q::fn<void, X1> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< std::tuple<X1, X2>, void, Q >, std::tuple< Q::fn<void, X1>, Q::fn<Q::fn<void, X1>, X2> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< std::tuple<X1, X2, X3>, void, Q >, std::tuple< Q::fn<void, X1>, Q::fn<Q::fn<void, X1>, X2>, Q::fn<Q::fn<Q::fn<void, X1>, X2>, X3> > >));

        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< mp_rename<mp_list_c<int, 7, 7, 2>, std::tuple>, mp_int<0>, mp_quote<mp_plus> >, std::tuple< mp_int<7>, mp_int<14>, mp_int<16> > >));
        BOOST_TEST_TRAIT_TRUE((std::is_same< mp_partial_sum_q< mp_rename<mp_list_c<std::size_t, 7, 7, 2>, std::tuple>, mp_size_t<0>, mp_quote<mp_plus> >, std::tuple< mp_size_t<7>, mp_size_t<14>, mp_size_t<16> > >));
    }

    return boost::report_errors();
}
