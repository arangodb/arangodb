
// Copyright 2017, 2020 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>

template<class T> struct F1 {};
template<class T> struct F2 {};
template<class T> struct F3 {};

template<class T> using G1 = F1<T>;
template<class T> using G2 = F2<T>;
template<class T> using G3 = F3<T>;

int main()
{
    using namespace boost::mp11;

#if !BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, < 1900 )

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_compose<>::fn<void>, void>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_compose<F1>::fn<void>, F1<void>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_compose<G1>::fn<void>, F1<void>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_compose<F1, F2>::fn<void>, F2<F1<void>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_compose<G1, G2>::fn<void>, F2<F1<void>>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_compose<F1, F2, F3>::fn<void>, F3<F2<F1<void>>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_compose<G1, G2, G3>::fn<void>, F3<F2<F1<void>>>>));

#endif

    using QF1 = mp_quote<F1>;
    using QF2 = mp_quote<F2>;
    using QF3 = mp_quote<F3>;

    using QG1 = mp_quote<G1>;
    using QG2 = mp_quote<G2>;
    using QG3 = mp_quote<G3>;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_compose_q<>::fn<void>, void>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_compose_q<QF1>::fn<void>, F1<void>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_compose_q<QG1>::fn<void>, F1<void>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_compose_q<QF1, QF2>::fn<void>, F2<F1<void>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_compose_q<QG1, QG2>::fn<void>, F2<F1<void>>>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_compose_q<QF1, QF2, QF3>::fn<void>, F3<F2<F1<void>>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_compose_q<QG1, QG2, QG3>::fn<void>, F3<F2<F1<void>>>>));

    //

    return boost::report_errors();
}
