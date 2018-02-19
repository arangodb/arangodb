
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

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_erase;
    using boost::mp11::mp_erase_c;
    using boost::mp11::mp_size_t;

    using _0 = mp_size_t<0>;
    using _1 = mp_size_t<1>;
    using _2 = mp_size_t<2>;
    using _3 = mp_size_t<3>;
    using _4 = mp_size_t<4>;
    using _5 = mp_size_t<5>;

    {
        using L1 = mp_list<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L1, 0, 0>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L1, _0, _0>, L1>));

        using L2 = mp_list<X1, X2, X3, X4, X5>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 0, 0>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 1, 1>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 2, 2>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 3, 3>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 4, 4>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 5, 5>, L2>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _0, _0>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _1, _1>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _2, _2>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _3, _3>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _4, _4>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _5, _5>, L2>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 0, 1>, mp_list<X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 1, 2>, mp_list<X1, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 2, 3>, mp_list<X1, X2, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 3, 4>, mp_list<X1, X2, X3, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 4, 5>, mp_list<X1, X2, X3, X4>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _0, _1>, mp_list<X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _1, _2>, mp_list<X1, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _2, _3>, mp_list<X1, X2, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _3, _4>, mp_list<X1, X2, X3, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _4, _5>, mp_list<X1, X2, X3, X4>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 0, 2>, mp_list<X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 1, 3>, mp_list<X1, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 2, 4>, mp_list<X1, X2, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 3, 5>, mp_list<X1, X2, X3>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _0, _2>, mp_list<X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _1, _3>, mp_list<X1, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _2, _4>, mp_list<X1, X2, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _3, _5>, mp_list<X1, X2, X3>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 0, 3>, mp_list<X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 1, 4>, mp_list<X1, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 2, 5>, mp_list<X1, X2>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _0, _3>, mp_list<X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _1, _4>, mp_list<X1, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _2, _5>, mp_list<X1, X2>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 0, 4>, mp_list<X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 1, 5>, mp_list<X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 0, 5>, mp_list<>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _0, _4>, mp_list<X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _1, _5>, mp_list<X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _0, _5>, mp_list<>>));

        using L3 = mp_list<X1, X2, X3, X4, X5, X1, X2, X3, X4, X5, X1, X2, X3, X4, X5, X1, X2, X3, X4, X5>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L3, 8, 18>, mp_list<X1, X2, X3, X4, X5, X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L3, mp_size_t<8>, mp_size_t<18>>, mp_list<X1, X2, X3, X4, X5, X1, X2, X3, X4, X5>>));
    }

    {
        using L1 = std::tuple<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L1, 0, 0>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L1, _0, _0>, L1>));

        using L2 = std::tuple<X1, X2, X3, X4, X5>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 0, 0>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 1, 1>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 2, 2>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 3, 3>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 4, 4>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 5, 5>, L2>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _0, _0>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _1, _1>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _2, _2>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _3, _3>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _4, _4>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _5, _5>, L2>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 0, 1>, std::tuple<X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 1, 2>, std::tuple<X1, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 2, 3>, std::tuple<X1, X2, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 3, 4>, std::tuple<X1, X2, X3, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 4, 5>, std::tuple<X1, X2, X3, X4>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _0, _1>, std::tuple<X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _1, _2>, std::tuple<X1, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _2, _3>, std::tuple<X1, X2, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _3, _4>, std::tuple<X1, X2, X3, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _4, _5>, std::tuple<X1, X2, X3, X4>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 0, 2>, std::tuple<X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 1, 3>, std::tuple<X1, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 2, 4>, std::tuple<X1, X2, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 3, 5>, std::tuple<X1, X2, X3>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _0, _2>, std::tuple<X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _1, _3>, std::tuple<X1, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _2, _4>, std::tuple<X1, X2, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _3, _5>, std::tuple<X1, X2, X3>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 0, 3>, std::tuple<X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 1, 4>, std::tuple<X1, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 2, 5>, std::tuple<X1, X2>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _0, _3>, std::tuple<X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _1, _4>, std::tuple<X1, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _2, _5>, std::tuple<X1, X2>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 0, 4>, std::tuple<X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 1, 5>, std::tuple<X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L2, 0, 5>, std::tuple<>>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _0, _4>, std::tuple<X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _1, _5>, std::tuple<X1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L2, _0, _5>, std::tuple<>>));

        using L3 = std::tuple<X1, X2, X3, X4, X5, X1, X2, X3, X4, X5, X1, X2, X3, X4, X5, X1, X2, X3, X4, X5>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase_c<L3, 8, 18>, std::tuple<X1, X2, X3, X4, X5, X1, X2, X3, X4, X5>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_erase<L3, mp_size_t<8>, mp_size_t<18>>, std::tuple<X1, X2, X3, X4, X5, X1, X2, X3, X4, X5>>));
    }

    return boost::report_errors();
}
