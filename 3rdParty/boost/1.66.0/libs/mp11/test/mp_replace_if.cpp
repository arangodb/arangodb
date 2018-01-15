
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

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_replace_if;

    {
        using L1 = mp_list<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_if<L1, std::is_const, void>, L1>));

        using L2 = mp_list<X1, X1 const, X1*, X1 const, X1*, X1*>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_if<L2, std::is_volatile, void>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_if<L2, std::is_const, void>, mp_list<X1, void, X1*, void, X1*, X1*>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_if<L2, std::is_pointer, void>, mp_list<X1, X1 const, void, X1 const, void, void>>));
    }

    {
        using L1 = std::tuple<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_if<L1, std::is_const, void>, L1>));

        using L2 = std::tuple<X1, X1 const, X1*, X1 const, X1*, X1*>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_if<L2, std::is_volatile, void>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_if<L2, std::is_const, void>, std::tuple<X1, void, X1*, void, X1*, X1*>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_if<L2, std::is_pointer, void>, std::tuple<X1, X1 const, void, X1 const, void, void>>));
    }

    {
        using L2 = std::pair<X1 const, X1*>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_if<L2, std::is_volatile, void>, L2>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_if<L2, std::is_const, void>, std::pair<void, X1*>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_replace_if<L2, std::is_pointer, void>, std::pair<X1 const, void>>));
    }

    return boost::report_errors();
}
