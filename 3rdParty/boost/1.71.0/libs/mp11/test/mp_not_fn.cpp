
// Copyright 2019 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

template<class T> using is_const_ = std::is_const<T>;

struct Q_is_const
{
    template<class T> using fn = std::is_const<T>;
};

int main()
{
    using boost::mp11::mp_not_fn;
    using boost::mp11::mp_not_fn_q;

    BOOST_TEST_TRAIT_TRUE((mp_not_fn<std::is_const>::fn<void>));
    BOOST_TEST_TRAIT_FALSE((mp_not_fn<std::is_const>::fn<void const>));

    BOOST_TEST_TRAIT_TRUE((mp_not_fn<is_const_>::fn<void>));
    BOOST_TEST_TRAIT_FALSE((mp_not_fn<is_const_>::fn<void const>));

    BOOST_TEST_TRAIT_TRUE((mp_not_fn_q<Q_is_const>::fn<void>));
    BOOST_TEST_TRAIT_FALSE((mp_not_fn_q<Q_is_const>::fn<void const>));

    return boost::report_errors();
}
