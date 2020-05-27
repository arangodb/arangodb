
//  Copyright 2015, 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

struct X1 {};
struct X2 {};
struct X3 {};
struct X4 {};
struct X5 {};

struct Y1 {};
struct Y2 {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_insert;
    using boost::mp11::mp_insert_c;
    using boost::mp11::mp_size_t;

    {
        using L1 = mp_list<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L1, 0>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L1, mp_size_t<0>>, L1>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L1, 0, Y1>, mp_list<Y1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L1, mp_size_t<0>, Y1>, mp_list<Y1>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L1, 0, Y1, Y2>, mp_list<Y1, Y2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L1, mp_size_t<0>, Y1, Y2>, mp_list<Y1, Y2>>));

        using L2 = mp_list<X1, X2, X3, X4, X5>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 0>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 1>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 2>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 3>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 4>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 5>, L2>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 0, Y1, Y2>, mp_list<Y1, Y2, X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 1, Y1, Y2>, mp_list<X1, Y1, Y2, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 2, Y1, Y2>, mp_list<X1, X2, Y1, Y2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 3, Y1, Y2>, mp_list<X1, X2, X3, Y1, Y2, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 4, Y1, Y2>, mp_list<X1, X2, X3, X4, Y1, Y2, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 5, Y1, Y2>, mp_list<X1, X2, X3, X4, X5, Y1, Y2>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<0>>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<1>>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<2>>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<3>>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<4>>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<5>>, L2>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<0>, Y1, Y2>, mp_list<Y1, Y2, X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<1>, Y1, Y2>, mp_list<X1, Y1, Y2, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<2>, Y1, Y2>, mp_list<X1, X2, Y1, Y2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<3>, Y1, Y2>, mp_list<X1, X2, X3, Y1, Y2, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<4>, Y1, Y2>, mp_list<X1, X2, X3, X4, Y1, Y2, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<5>, Y1, Y2>, mp_list<X1, X2, X3, X4, X5, Y1, Y2>>));

        using L3 = mp_list<X1, X2, X3, X4, X5, X1, X2, X3, X4, X5, X1, X2, X3, X4, X5, X1, X2, X3, X4, X5>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L3, 8>, L3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L3, mp_size_t<9>>, L3>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L3, 8, Y1, Y2>, mp_list<X1, X2, X3, X4, X5, X1, X2, X3, Y1, Y2, X4, X5, X1, X2, X3, X4, X5, X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L3, mp_size_t<9>, Y1, Y2>, mp_list<X1, X2, X3, X4, X5, X1, X2, X3, X4, Y1, Y2, X5, X1, X2, X3, X4, X5, X1, X2, X3, X4, X5>>));
    }

    {
        using L1 = std::tuple<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L1, 0>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L1, mp_size_t<0>>, L1>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L1, 0, Y1>, std::tuple<Y1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L1, mp_size_t<0>, Y1>, std::tuple<Y1>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L1, 0, Y1, Y2>, std::tuple<Y1, Y2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L1, mp_size_t<0>, Y1, Y2>, std::tuple<Y1, Y2>>));

        using L2 = std::tuple<X1, X2, X3, X4, X5>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 0>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 1>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 2>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 3>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 4>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 5>, L2>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 0, Y1, Y2>, std::tuple<Y1, Y2, X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 1, Y1, Y2>, std::tuple<X1, Y1, Y2, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 2, Y1, Y2>, std::tuple<X1, X2, Y1, Y2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 3, Y1, Y2>, std::tuple<X1, X2, X3, Y1, Y2, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 4, Y1, Y2>, std::tuple<X1, X2, X3, X4, Y1, Y2, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L2, 5, Y1, Y2>, std::tuple<X1, X2, X3, X4, X5, Y1, Y2>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<0>>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<1>>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<2>>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<3>>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<4>>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<5>>, L2>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<0>, Y1, Y2>, std::tuple<Y1, Y2, X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<1>, Y1, Y2>, std::tuple<X1, Y1, Y2, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<2>, Y1, Y2>, std::tuple<X1, X2, Y1, Y2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<3>, Y1, Y2>, std::tuple<X1, X2, X3, Y1, Y2, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<4>, Y1, Y2>, std::tuple<X1, X2, X3, X4, Y1, Y2, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L2, mp_size_t<5>, Y1, Y2>, std::tuple<X1, X2, X3, X4, X5, Y1, Y2>>));

        using L3 = std::tuple<X1, X2, X3, X4, X5, X1, X2, X3, X4, X5, X1, X2, X3, X4, X5, X1, X2, X3, X4, X5>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L3, 8>, L3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L3, mp_size_t<9>>, L3>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert_c<L3, 8, Y1, Y2>, std::tuple<X1, X2, X3, X4, X5, X1, X2, X3, Y1, Y2, X4, X5, X1, X2, X3, X4, X5, X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_insert<L3, mp_size_t<9>, Y1, Y2>, std::tuple<X1, X2, X3, X4, X5, X1, X2, X3, X4, Y1, Y2, X5, X1, X2, X3, X4, X5, X1, X2, X3, X4, X5>>));
    }

    return boost::report_errors();
}
