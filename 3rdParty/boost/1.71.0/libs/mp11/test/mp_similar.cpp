
// Copyright 2019 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/function.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

template<class> class X;
template<class...> class Y;

int main()
{
    using boost::mp11::mp_similar;
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<void, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<void, void, void, void>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<void, void, void, void, void>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<void, int>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<void, void, int>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<void, void, void, int>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<void, void, void, void, int>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<X<void>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<X<void>, X<int>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<X<void>, X<int>, X<float>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<Y<>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<Y<>, Y<void>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<Y<>, Y<void>, Y<void, void>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_similar<Y<>, Y<void>, Y<void, void>, Y<void, void, void>>, mp_true>));

    return boost::report_errors();
}
