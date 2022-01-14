
// Copyright 2019 Peter Dimov.
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

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_flatten;
    using boost::mp11::mp_transform;

    {
        using L1 = mp_list<>;
        using L2 = mp_list<void>;
        using L3 = mp_list<void, void>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L1>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L2>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L3>, L3>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L1, L1>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L2, L2>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L3, L3>, L3>));

        using L4 = mp_transform<mp_list, L3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L4>, L3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L4, mp_list<>>, L3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L4, std::tuple<>>, L4>));

        using L5 = mp_transform<std::tuple, L3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L5>, L5>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L5, mp_list<>>, L5>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L5, std::tuple<>>, L3>));
    }

    {
        using L1 = std::tuple<>;
        using L2 = std::tuple<void>;
        using L3 = std::tuple<void, void>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L1>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L2>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L3>, L3>));

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L1, L1>, L1>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L2, L2>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L3, L3>, L3>));

        using L4 = mp_transform<mp_list, L3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L4>, L4>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L4, mp_list<>>, L3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L4, std::tuple<>>, L4>));

        using L5 = mp_transform<std::tuple, L3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L5>, L3>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L5, mp_list<>>, L5>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_flatten<L5, std::tuple<>>, L3>));
    }

    {
        using L1 = mp_list<std::tuple<>, int, mp_list<>, void, mp_list<char, double>, std::pair<int, void>>;

        using R1 = mp_flatten<L1>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R1, mp_list<std::tuple<>, int, void, char, double, std::pair<int, void>>>));

        using R2 = mp_flatten<L1, std::pair<void, void>>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R2, mp_list<std::tuple<>, int, mp_list<>, void, mp_list<char, double>, int, void>>));

        using R3 = mp_flatten<L1, std::tuple<>>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R3, mp_list<int, mp_list<>, void, mp_list<char, double>, std::pair<int, void>>>));
    }

    {
        using L1 = std::tuple<std::tuple<>, int, mp_list<>, void, mp_list<char, double>, std::pair<int, void>>;

        using R1 = mp_flatten<L1, mp_list<void, void, void, void>>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R1, std::tuple<std::tuple<>, int, void, char, double, std::pair<int, void>>>));

        using R2 = mp_flatten<L1, std::pair<void, void>>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R2, std::tuple<std::tuple<>, int, mp_list<>, void, mp_list<char, double>, int, void>>));

        using R3 = mp_flatten<L1>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R3, std::tuple<int, mp_list<>, void, mp_list<char, double>, std::pair<int, void>>>));
    }

    return boost::report_errors();
}
