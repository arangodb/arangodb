
//  Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

struct X1 {};
struct X2 {};
struct X3 {};
struct X4 {};

struct Y1 {};
struct Y2 {};
struct Y3 {};
struct Y4 {};

struct Z1 {};
struct Z2 {};
struct Z3 {};
struct Z4 {};

struct U1 {};
struct U2 {};

struct V1 {};
struct V2 {};

struct W1 {};
struct W2 {};

template<class T> using add_pointer = typename std::add_pointer<T>::type;
template<class T, class U> using is_same = typename std::is_same<T, U>::type;

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_transform;

    using L1 = mp_list<X1, X2, X3, X4>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<mp_list, L1>, mp_list<mp_list<X1>, mp_list<X2>, mp_list<X3>, mp_list<X4>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<std::tuple, L1>, mp_list<std::tuple<X1>, std::tuple<X2>, std::tuple<X3>, std::tuple<X4>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<std::add_pointer, L1>, mp_list<std::add_pointer<X1>, std::add_pointer<X2>, std::add_pointer<X3>, std::add_pointer<X4>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<add_pointer, L1>, mp_list<X1*, X2*, X3*, X4*>>));

    using L2 = std::tuple<Y1, Y2, Y3, Y4>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<mp_list, L1, L2>, mp_list<mp_list<X1, Y1>, mp_list<X2, Y2>, mp_list<X3, Y3>, mp_list<X4, Y4>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<std::tuple, L1, L2>, mp_list<std::tuple<X1, Y1>, std::tuple<X2, Y2>, std::tuple<X3, Y3>, std::tuple<X4, Y4>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<std::pair, L1, L2>, mp_list<std::pair<X1, Y1>, std::pair<X2, Y2>, std::pair<X3, Y3>, std::pair<X4, Y4>>>));

    using L3 = mp_list<Z1, Z2, Z3, Z4>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<mp_list, L1, L2, L3>, mp_list<mp_list<X1, Y1, Z1>, mp_list<X2, Y2, Z2>, mp_list<X3, Y3, Z3>, mp_list<X4, Y4, Z4>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<std::tuple, L1, L2, L3>, mp_list<std::tuple<X1, Y1, Z1>, std::tuple<X2, Y2, Z2>, std::tuple<X3, Y3, Z3>, std::tuple<X4, Y4, Z4>>>));

    //

    using L4 = std::tuple<X1, Y2, X3, Y4>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<is_same, L1, L1>, mp_list<std::true_type, std::true_type, std::true_type, std::true_type>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<is_same, L1, L2>, mp_list<std::false_type, std::false_type, std::false_type, std::false_type>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<is_same, L1, L4>, mp_list<std::true_type, std::false_type, std::true_type, std::false_type>>));

    //

    using L5 = std::pair<X1, X2>;
    using L6 = std::pair<Y1, Y2>;
    using L7 = std::pair<X1, Y2>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<mp_list, L5>, std::pair<mp_list<X1>, mp_list<X2>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<mp_list, L5, L6>, std::pair<mp_list<X1, Y1>, mp_list<X2, Y2>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<mp_list, L5, L6, L7>, std::pair<mp_list<X1, Y1, X1>, mp_list<X2, Y2, Y2>>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<add_pointer, L5>, std::pair<X1*, X2*>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<is_same, L5, L5>, std::pair<std::true_type, std::true_type>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<is_same, L5, L6>, std::pair<std::false_type, std::false_type>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<is_same, L5, L7>, std::pair<std::true_type, std::false_type>>));

    //

    using L8 = std::pair<Z1, Z2>;
    using L9 = std::pair<U1, U2>;
    using L10 = std::pair<V1, V2>;
    using L11 = std::pair<W1, W2>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<std::tuple, L5, L6, L8, L9>, std::pair<std::tuple<X1, Y1, Z1, U1>, std::tuple<X2, Y2, Z2, U2>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<std::tuple, L5, L6, L8, L9, L10>, std::pair<std::tuple<X1, Y1, Z1, U1, V1>, std::tuple<X2, Y2, Z2, U2, V2>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform<std::tuple, L5, L6, L8, L9, L10, L11>, std::pair<std::tuple<X1, Y1, Z1, U1, V1, W1>, std::tuple<X2, Y2, Z2, U2, V2, W2>>>));

    //

    return boost::report_errors();
}
