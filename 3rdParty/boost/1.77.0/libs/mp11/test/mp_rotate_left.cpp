
// Copyright 2015 Peter Dimov.
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

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_rotate_left;
    using boost::mp11::mp_rotate_left_c;
    using boost::mp11::mp_size_t;

    {
        using L1 = mp_list<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 0>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 1>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 2>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 3>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 4>, L1>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<0>>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<1>>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<2>>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<3>>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<4>>, L1>));

        using L2 = mp_list<X1, X2, X3, X4, X5>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 0>, mp_list<X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 1>, mp_list<X2, X3, X4, X5, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 2>, mp_list<X3, X4, X5, X1, X2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 3>, mp_list<X4, X5, X1, X2, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 4>, mp_list<X5, X1, X2, X3, X4>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 5>, mp_list<X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 6>, mp_list<X2, X3, X4, X5, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 7>, mp_list<X3, X4, X5, X1, X2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 8>, mp_list<X4, X5, X1, X2, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 9>, mp_list<X5, X1, X2, X3, X4>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 10>, mp_list<X1, X2, X3, X4, X5>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<0>>, mp_list<X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<1>>, mp_list<X2, X3, X4, X5, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<2>>, mp_list<X3, X4, X5, X1, X2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<3>>, mp_list<X4, X5, X1, X2, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<4>>, mp_list<X5, X1, X2, X3, X4>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<5>>, mp_list<X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<6>>, mp_list<X2, X3, X4, X5, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<7>>, mp_list<X3, X4, X5, X1, X2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<8>>, mp_list<X4, X5, X1, X2, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<9>>, mp_list<X5, X1, X2, X3, X4>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<10>>, mp_list<X1, X2, X3, X4, X5>>));
    }

    {
        using L1 = std::tuple<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 0>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 1>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 2>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 3>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 4>, L1>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<0>>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<1>>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<2>>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<3>>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<4>>, L1>));

        using L2 = std::tuple<X1, X2, X3, X4, X5>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 0>, std::tuple<X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 1>, std::tuple<X2, X3, X4, X5, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 2>, std::tuple<X3, X4, X5, X1, X2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 3>, std::tuple<X4, X5, X1, X2, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 4>, std::tuple<X5, X1, X2, X3, X4>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 5>, std::tuple<X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 6>, std::tuple<X2, X3, X4, X5, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 7>, std::tuple<X3, X4, X5, X1, X2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 8>, std::tuple<X4, X5, X1, X2, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 9>, std::tuple<X5, X1, X2, X3, X4>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L2, 10>, std::tuple<X1, X2, X3, X4, X5>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<0>>, std::tuple<X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<1>>, std::tuple<X2, X3, X4, X5, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<2>>, std::tuple<X3, X4, X5, X1, X2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<3>>, std::tuple<X4, X5, X1, X2, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<4>>, std::tuple<X5, X1, X2, X3, X4>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<5>>, std::tuple<X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<6>>, std::tuple<X2, X3, X4, X5, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<7>>, std::tuple<X3, X4, X5, X1, X2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<8>>, std::tuple<X4, X5, X1, X2, X3>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<9>>, std::tuple<X5, X1, X2, X3, X4>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L2, mp_size_t<10>>, std::tuple<X1, X2, X3, X4, X5>>));
    }

    {
        using L1 = std::pair<X1, X2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 0>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 2>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 4>, L1>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 1>, std::pair<X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 3>, std::pair<X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left_c<L1, 5>, std::pair<X2, X1>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<0>>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<2>>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<4>>, L1>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<1>>, std::pair<X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<3>>, std::pair<X2, X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_rotate_left<L1, mp_size_t<5>>, std::pair<X2, X1>>));
    }

    return boost::report_errors();
}
