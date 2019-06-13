
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

using boost::mp11::mp_not;

template<class T> using add_pointer = T*;
template<class T, class...> using is_not_ref = mp_not<std::is_reference<T>>;
template<class T1, class T2> using second = T2;
template<class T1, class T2, class T3> using third = T3;
template<class T1, class T2, class T3, class T4> using fourth = T4;
template<class T1, class T2, class T3, class T4, class T5> using fifth = T5;

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_transform_if;
    using boost::mp11::mp_size_t;
    using boost::mp11::mp_size;
    using boost::mp11::mp_fill;
    using boost::mp11::mp_iota;

    using L1 = mp_list<X1, X2&, X3 const, X4 const&>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_if<is_not_ref, add_pointer, L1>, mp_list<X1*, X2&, X3 const*, X4 const&>>));

    using L2 = std::tuple<X1, X2&, X3 const, X4 const&>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_if<is_not_ref, add_pointer, L2>, std::tuple<X1*, X2&, X3 const*, X4 const&>>));

    using L3 = std::pair<X1 const, X2&>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_if<is_not_ref, add_pointer, L3>, std::pair<X1 const*, X2&>>));

    //

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_if<is_not_ref, second, L1, mp_fill<L1, void>>, mp_list<void, X2&, void, X4 const&>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_if<is_not_ref, second, L2, mp_fill<L2, void>>, std::tuple<void, X2&, void, X4 const&>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_if<is_not_ref, second, L3, mp_fill<L3, void>>, std::pair<void, X2&>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_if<is_not_ref, third, L1, L1, mp_iota<mp_size<L1>>>, mp_list<mp_size_t<0>, X2&, mp_size_t<2>, X4 const&>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_if<is_not_ref, third, L2, L2, mp_iota<mp_size<L2>>>, std::tuple<mp_size_t<0>, X2&, mp_size_t<2>, X4 const&>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_if<is_not_ref, third, L3, L3, mp_iota<mp_size<L3>>>, std::pair<mp_size_t<0>, X2&>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_if<is_not_ref, fourth, L1, L1, L1, mp_iota<mp_size<L1>>>, mp_list<mp_size_t<0>, X2&, mp_size_t<2>, X4 const&>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_if<is_not_ref, fourth, L2, L2, L2, mp_iota<mp_size<L2>>>, std::tuple<mp_size_t<0>, X2&, mp_size_t<2>, X4 const&>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_if<is_not_ref, fourth, L3, L3, L3, mp_iota<mp_size<L3>>>, std::pair<mp_size_t<0>, X2&>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_if<is_not_ref, fifth, L1, L1, L1, L1, mp_iota<mp_size<L1>>>, mp_list<mp_size_t<0>, X2&, mp_size_t<2>, X4 const&>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_if<is_not_ref, fifth, L2, L2, L2, L2, mp_iota<mp_size<L2>>>, std::tuple<mp_size_t<0>, X2&, mp_size_t<2>, X4 const&>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_transform_if<is_not_ref, fifth, L3, L3, L3, L3, mp_iota<mp_size<L3>>>, std::pair<mp_size_t<0>, X2&>>));

    //

    return boost::report_errors();
}
