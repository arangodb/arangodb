
//  Copyright 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/detail/config.hpp>
#include <boost/core/lightweight_test_trait.hpp>

using boost::mp11::mp_transform;
using boost::mp11::mp_list;
using boost::mp11::mp_valid;

template<class...> using F = void;

template<class... L> using transform = mp_transform<F, L...>;

int main()
{
    BOOST_TEST_TRAIT_FALSE((mp_valid<transform>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<transform, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<transform, void, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<transform, void, void, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<transform, void, void, void, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<transform, void, void, void, void, void>));

#if !BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, <= 1800 )
    BOOST_TEST_TRAIT_TRUE((mp_valid<transform, mp_list<>>));
#endif

    BOOST_TEST_TRAIT_FALSE((mp_valid<transform, mp_list<>, mp_list<void>>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<transform, mp_list<>, mp_list<>, mp_list<void>>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<transform, mp_list<>, mp_list<>, mp_list<>, mp_list<void>>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<transform, mp_list<>, mp_list<>, mp_list<>, mp_list<>, mp_list<void>>));

    return boost::report_errors();
}
