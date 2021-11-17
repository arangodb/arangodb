
//  Copyright 2015 Peter Dimov.
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

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_repeat;
    using boost::mp11::mp_repeat_c;
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;
    using boost::mp11::mp_int;
    using boost::mp11::mp_size_t;

    using L1 = mp_list<>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L1, 0>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L1, 1>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L1, 2>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L1, 3>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L1, 31>, mp_list<>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L1, mp_false>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L1, mp_true>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L1, mp_int<2>>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L1, mp_int<3>>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L1, mp_size_t<31>>, mp_list<>>));

    using L2 = mp_list<X1>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L2, 0>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L2, 1>, mp_list<X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L2, 2>, mp_list<X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L2, 3>, mp_list<X1, X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L2, 4>, mp_list<X1, X1, X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L2, 5>, mp_list<X1, X1, X1, X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L2, 6>, mp_list<X1, X1, X1, X1, X1, X1>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L2, mp_false>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L2, mp_true>, mp_list<X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L2, mp_int<2>>, mp_list<X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L2, mp_int<3>>, mp_list<X1, X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L2, mp_int<4>>, mp_list<X1, X1, X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L2, mp_size_t<5>>, mp_list<X1, X1, X1, X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L2, mp_size_t<6>>, mp_list<X1, X1, X1, X1, X1, X1>>));

    using L3 = mp_list<X1, X2>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L3, 0>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L3, 1>, mp_list<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L3, 2>, mp_list<X1, X2, X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L3, 3>, mp_list<X1, X2, X1, X2, X1, X2>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L3, mp_false>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L3, mp_true>, mp_list<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L3, mp_int<2>>, mp_list<X1, X2, X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L3, mp_size_t<3>>, mp_list<X1, X2, X1, X2, X1, X2>>));

    //

    using L4 = std::tuple<>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L4, 0>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L4, 1>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L4, 2>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L4, 3>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L4, 31>, std::tuple<>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L4, mp_false>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L4, mp_true>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L4, mp_int<2>>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L4, mp_int<3>>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L4, mp_size_t<31>>, std::tuple<>>));

    using L5 = std::tuple<X1>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L5, 0>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L5, 1>, std::tuple<X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L5, 2>, std::tuple<X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L5, 3>, std::tuple<X1, X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L5, 4>, std::tuple<X1, X1, X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L5, 5>, std::tuple<X1, X1, X1, X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L5, 6>, std::tuple<X1, X1, X1, X1, X1, X1>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L5, mp_false>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L5, mp_true>, std::tuple<X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L5, mp_int<2>>, std::tuple<X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L5, mp_int<3>>, std::tuple<X1, X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L5, mp_int<4>>, std::tuple<X1, X1, X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L5, mp_size_t<5>>, std::tuple<X1, X1, X1, X1, X1>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L5, mp_size_t<6>>, std::tuple<X1, X1, X1, X1, X1, X1>>));

    using L6 = std::tuple<X1, X2>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L6, 0>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L6, 1>, std::tuple<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L6, 2>, std::tuple<X1, X2, X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L6, 3>, std::tuple<X1, X2, X1, X2, X1, X2>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L6, mp_false>, std::tuple<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L6, mp_true>, std::tuple<X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L6, mp_int<2>>, std::tuple<X1, X2, X1, X2>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L6, mp_size_t<3>>, std::tuple<X1, X2, X1, X2, X1, X2>>));

    //

    using L7 = std::pair<X1, X2>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat_c<L7, 1>, std::pair<X1, X2>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_repeat<L7, mp_true>, std::pair<X1, X2>>));

    //

    return boost::report_errors();
}
