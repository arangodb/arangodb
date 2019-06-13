
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
struct X3 {};
struct X4 {};
struct X5 {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_at;
    using boost::mp11::mp_at_c;
    using boost::mp11::mp_size_t;

    {
        using L1 = mp_list<X1, X2, X3, X4, X5>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at_c<L1, 0>, X1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at_c<L1, 1>, X2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at_c<L1, 2>, X3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at_c<L1, 3>, X4>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at_c<L1, 4>, X5>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at<L1, mp_size_t<0>>, X1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at<L1, mp_size_t<1>>, X2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at<L1, mp_size_t<2>>, X3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at<L1, mp_size_t<3>>, X4>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at<L1, mp_size_t<4>>, X5>));
    }

    {
        using L1 = std::tuple<X1, X2, X3, X4, X5>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at_c<L1, 0>, X1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at_c<L1, 1>, X2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at_c<L1, 2>, X3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at_c<L1, 3>, X4>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at_c<L1, 4>, X5>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at<L1, mp_size_t<0>>, X1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at<L1, mp_size_t<1>>, X2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at<L1, mp_size_t<2>>, X3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at<L1, mp_size_t<3>>, X4>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at<L1, mp_size_t<4>>, X5>));
    }

    {
        using L1 = std::pair<X1, X2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at_c<L1, 0>, X1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at_c<L1, 1>, X2>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at<L1, mp_size_t<0>>, X1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_at<L1, mp_size_t<1>>, X2>));
    }

    return boost::report_errors();
}
