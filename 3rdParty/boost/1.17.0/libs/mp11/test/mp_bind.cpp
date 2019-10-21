
// Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/bind.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

struct X1 {};
struct X2 {};
struct X3 {};
struct X4 {};
struct X5 {};
struct X6 {};
struct X7 {};
struct X8 {};
struct X9 {};

template<class T> using add_pointer = T*;

int main()
{
    using namespace boost::mp11;

    BOOST_TEST_TRAIT_TRUE((std::is_same<_1::fn<X1>, X1>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<_1::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X1>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<_2::fn<X1, X2>, X2>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<_2::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X2>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<_3::fn<X1, X2, X3>, X3>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<_3::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X3>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<_4::fn<X1, X2, X3, X4>, X4>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<_4::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X4>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<_5::fn<X1, X2, X3, X4, X5>, X5>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<_5::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X5>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<_6::fn<X1, X2, X3, X4, X5, X6>, X6>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<_6::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X6>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<_7::fn<X1, X2, X3, X4, X5, X6, X7>, X7>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<_7::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X7>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<_8::fn<X1, X2, X3, X4, X5, X6, X7, X8>, X8>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<_8::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X8>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<_9::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X9>));

    //

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _1>::fn<X1>, X1*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _1>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X1*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _2>::fn<X1, X2>, X2*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _2>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X2*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _3>::fn<X1, X2, X3>, X3*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _3>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X3*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _4>::fn<X1, X2, X3, X4>, X4*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _4>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X4*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _5>::fn<X1, X2, X3, X4, X5>, X5*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _5>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X5*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _6>::fn<X1, X2, X3, X4, X5, X6>, X6*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _6>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X6*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _7>::fn<X1, X2, X3, X4, X5, X6, X7>, X7*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _7>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X7*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _8>::fn<X1, X2, X3, X4, X5, X6, X7, X8>, X8*>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _8>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X8*>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_bind<add_pointer, _9>::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, X9*>));

    //

    BOOST_TEST_TRAIT_TRUE((std::is_same<
        mp_bind<std::tuple,
            mp_bind<add_pointer, _9>,
            mp_bind<add_pointer, _8>,
            mp_bind<add_pointer, _7>,
            mp_bind<add_pointer, _6>,
            mp_bind<add_pointer, _5>,
            mp_bind<add_pointer, _4>,
            mp_bind<add_pointer, _3>,
            mp_bind<add_pointer, _2>,
            mp_bind<add_pointer, _1>
        >::fn<X1, X2, X3, X4, X5, X6, X7, X8, X9>, std::tuple<X9*, X8*, X7*, X6*, X5*, X4*, X3*, X2*, X1*>>));

    //

    return boost::report_errors();
}
