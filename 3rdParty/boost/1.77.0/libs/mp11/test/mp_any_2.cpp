// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/detail/config.hpp>

#if BOOST_MP11_MSVC
# pragma warning( disable: 4503 ) // decorated name length exceeded
#endif

#include <boost/mp11/function.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

int main()
{
    using boost::mp11::mp_any;

    using boost::mp11::mp_list;
    using boost::mp11::mp_apply;
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;

    using boost::mp11::mp_repeat_c;
    using boost::mp11::mp_push_back;

    int const N = 1089;

    using L1 = mp_repeat_c<mp_list<mp_false>, N>;
    using R1 = mp_apply<mp_any, L1>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<R1, mp_false>));

    using L2 = mp_push_back<L1, mp_true>;
    using R2 = mp_apply<mp_any, L2>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<R2, mp_true>));

    return boost::report_errors();
}
