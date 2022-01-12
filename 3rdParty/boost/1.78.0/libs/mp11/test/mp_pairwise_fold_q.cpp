// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11.hpp>
#include <boost/core/lightweight_test_trait.hpp>

using namespace boost::mp11;

template<class L, template<class...> class P> using is_sorted = mp_none_of<mp_pairwise_fold_q<L, mp_bind<P, _2, _1>>, mp_to_bool>;

int main()
{
    BOOST_TEST_TRAIT_TRUE((is_sorted<mp_list<mp_int<0>, mp_int<0>, mp_int<1>, mp_int<3>, mp_int<3>, mp_int<7>>, mp_less>));
    BOOST_TEST_TRAIT_FALSE((is_sorted<mp_list<mp_int<0>, mp_int<0>, mp_int<1>, mp_int<3>, mp_int<3>, mp_int<2>>, mp_less>));

    return boost::report_errors();
}
