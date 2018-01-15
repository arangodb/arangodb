
// Copyright 2015, 2016 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/function.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

using boost::mp11::mp_all;

template<class V, class... T> using check1 = mp_all<std::is_same<V, void>, std::is_copy_constructible<T>..., std::is_copy_assignable<T>...>;
template<class V, class... T> using check2 = mp_all<std::is_same<V, void>, mp_all<std::is_copy_constructible<T>..., mp_all<std::is_copy_assignable<T>...>>>;

int main()
{
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;
    using boost::mp11::mp_int;
    using boost::mp11::mp_size_t;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_true>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_false>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_int<-7>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_int<0>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_size_t<7>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_size_t<0>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_true, mp_true>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_false, mp_true>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_true, mp_false>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_false, mp_false>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_int<-7>, mp_int<7>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_size_t<(size_t)-1>, mp_size_t<1>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_true, mp_true, mp_true>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_true, mp_false, mp_true>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_false, mp_false, mp_false>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_true, mp_true, mp_true, mp_true>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_true, mp_false, mp_true, mp_true>, mp_false>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_false, mp_false, mp_false, mp_false>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_int<1>, mp_int<2>, mp_int<-11>, mp_int<14>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_int<1>, mp_int<0>, mp_int<-11>, mp_int<14>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_size_t<1>, mp_size_t<2>, mp_size_t<114>, mp_size_t<8>, mp_size_t<94>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_all<mp_size_t<1>, mp_size_t<2>, mp_size_t<114>, mp_size_t<0>, mp_size_t<94>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<check1<void, int, float>, mp_true>));

#if !BOOST_WORKAROUND( BOOST_GCC, < 40900 )

    BOOST_TEST_TRAIT_TRUE((std::is_same<check2<void, int, float>, mp_true>));

#endif

    return boost::report_errors();
}
