
// Copyright 2019 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <cstddef>

template<class T> using difference_type = typename T::difference_type;

struct X
{
};

struct Y
{
    using difference_type = int;
};

int main()
{
    using boost::mp11::mp_eval_or;
    using boost::mp11::mp_eval_or_q;
    using boost::mp11::mp_identity;
    using boost::mp11::mp_quote;

    BOOST_TEST_TRAIT_SAME(mp_eval_or<void, mp_identity>, void);
    BOOST_TEST_TRAIT_SAME(mp_eval_or<void, mp_identity, int>, mp_identity<int>);
    BOOST_TEST_TRAIT_SAME(mp_eval_or<void, mp_identity, int, int>, void);

    using Q_identity = mp_quote<mp_identity>;

    BOOST_TEST_TRAIT_SAME(mp_eval_or_q<void, Q_identity>, void);
    BOOST_TEST_TRAIT_SAME(mp_eval_or_q<void, Q_identity, int>, mp_identity<int>);
    BOOST_TEST_TRAIT_SAME(mp_eval_or_q<void, Q_identity, int, int>, void);

    BOOST_TEST_TRAIT_SAME(mp_eval_or<std::ptrdiff_t, difference_type, X>, std::ptrdiff_t);
    BOOST_TEST_TRAIT_SAME(mp_eval_or<std::ptrdiff_t, difference_type, Y>, int);

    using Q_diff_type = mp_quote<difference_type>;

    BOOST_TEST_TRAIT_SAME(mp_eval_or_q<std::ptrdiff_t, Q_diff_type, X>, std::ptrdiff_t);
    BOOST_TEST_TRAIT_SAME(mp_eval_or_q<std::ptrdiff_t, Q_diff_type, Y>, int);

    return boost::report_errors();
}
