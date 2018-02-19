
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/bind.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

struct X1 {};
struct X2 {};
struct X3 {};
struct X4 {};
struct X5 {};
struct X6 {};
struct X7 {};
struct X8 {};
struct X9 {};

template<class T> using add_pointer = typename std::add_pointer<T>::type;

int main()
{
    using namespace boost::mp11;

    using Q_addp = mp_quote<add_pointer>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _1>::fn<X1>, X1*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _1>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X1*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _2>::fn<X1, X2>, X2*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _2>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X2*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _3>::fn<X1, X2, X3>, X3*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _3>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X3*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _4>::fn<X1, X2, X3, X4>, X4*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _4>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X4*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _5>::fn<X1, X2, X3, X4, X5>, X5*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _5>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X5*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _6>::fn<X1, X2, X3, X4, X5, X6>, X6*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _6>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X6*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _7>::fn<X1, X2, X3, X4, X5, X6, X7>, X7*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _7>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X7*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _8>::fn<X1, X2, X3, X4, X5, X6, X7, X8>, X8*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _8>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X8*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind_q<Q_addp, _9>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X9*>));

    //

    return boost::report_errors();
}
