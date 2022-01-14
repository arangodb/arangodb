
// Copyright 2015, 2016, 2019 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/function.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/mp11/set.hpp>
#include <boost/mp11/detail/config.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

using boost::mp11::mp_any;
using boost::mp11::mp_all;

template<class... T> using check1 = mp_any<std::is_nothrow_copy_constructible<T>..., mp_any<std::is_nothrow_move_constructible<T>...>>;
template<class... T> using check2 = mp_any<mp_any<std::is_nothrow_copy_constructible<T>...>, std::is_nothrow_move_constructible<T>...>;
template<class... T> using check3 = mp_any<mp_all<std::is_nothrow_copy_constructible<T>...>, std::is_nothrow_default_constructible<T>...>;

template<bool is_trivially_destructible, bool is_single_buffered, class... T> struct variant_base_impl {};
#if BOOST_MP11_WORKAROUND( BOOST_MP11_GCC, < 40800 )
template<class... T> using variant_base = variant_base_impl<mp_all<std::has_trivial_destructor<T>...>::value, mp_any<mp_all<std::is_nothrow_move_constructible<T>...>, std::is_nothrow_default_constructible<T>...>::value, T...>;
#else
template<class... T> using variant_base = variant_base_impl<mp_all<std::is_trivially_destructible<T>...>::value, mp_any<mp_all<std::is_nothrow_move_constructible<T>...>, std::is_nothrow_default_constructible<T>...>::value, T...>;
#endif

using boost::mp11::mp_set_contains;

template<class T, class... S> using in_any_set = mp_any< mp_set_contains<S, T>... >;

int main()
{
    using boost::mp11::mp_true;
    using boost::mp11::mp_false;
    using boost::mp11::mp_int;
    using boost::mp11::mp_size_t;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_true>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_false>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_int<-7>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_int<0>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_size_t<7>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_size_t<0>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_true, mp_true>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_false, mp_true>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_true, mp_false>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_false, mp_false>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_int<-7>, mp_int<7>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_size_t<(size_t)-1>, mp_size_t<1>>, mp_true>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_true, mp_true, mp_true>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_false, mp_false, mp_true>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_false, mp_false, mp_false>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_true, mp_true, mp_true, mp_true>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_false, mp_false, mp_true, mp_false>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_false, mp_false, mp_false, mp_false>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_int<1>, mp_int<2>, mp_int<-11>, mp_int<14>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_int<0>, mp_int<0>, mp_int<-11>, mp_int<0>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_int<0>, mp_int<0>, mp_int<0>, mp_int<0>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_size_t<1>, mp_size_t<2>, mp_size_t<114>, mp_size_t<8>, mp_size_t<94>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_size_t<0>, mp_size_t<0>, mp_size_t<114>, mp_size_t<0>, mp_size_t<0>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_size_t<0>, mp_size_t<0>, mp_size_t<0>, mp_size_t<0>, mp_size_t<0>>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_size_t<0>, mp_int<0>, mp_false, mp_size_t<141>>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_any<mp_size_t<0>, mp_int<0>, mp_false>, mp_false>));

    BOOST_TEST_TRAIT_TRUE((std::is_same<check1<void, int, float>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<check2<void, int, float>, mp_true>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<check3<void, int, float>, mp_true>));

    variant_base<void, int, float>();

    using boost::mp11::mp_list;

    BOOST_TEST_TRAIT_TRUE((std::is_same<in_any_set<void, mp_list<void>, mp_list<int>>, mp_true>));

    return boost::report_errors();
}
