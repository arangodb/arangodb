
// Copyright 2015-2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/utility.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/mp11/detail/config.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

using boost::mp11::mp_identity;
using boost::mp11::mp_true;
using boost::mp11::mp_false;

template<class T> struct has_type
{
    template<class U> static mp_true f( mp_identity<typename U::type>* );
    template<class U> static mp_false f( ... );

    using type = decltype( f<T>(0) );

    static const bool value = type::value;
};

using boost::mp11::mp_defer;

template<class T> using add_pointer = T*;
template<class... T> using add_pointer_impl = mp_defer<add_pointer, T...>;

using boost::mp11::mp_quote;

using Q_add_pointer = mp_quote<add_pointer>;
template<class... T> using Q_add_pointer_impl = mp_defer<Q_add_pointer::fn, T...>;

int main()
{
    BOOST_TEST_TRAIT_TRUE((has_type<add_pointer_impl<void>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<add_pointer_impl<void>::type, void*>));

    BOOST_TEST_TRAIT_TRUE((has_type<add_pointer_impl<int>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<add_pointer_impl<int>::type, int*>));

    BOOST_TEST_TRAIT_FALSE((has_type<add_pointer_impl<>>));
    BOOST_TEST_TRAIT_FALSE((has_type<add_pointer_impl<void, void>>));

#if !BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, <= 1800 )
    BOOST_TEST_TRAIT_TRUE((has_type<Q_add_pointer_impl<void>>));
#endif
    BOOST_TEST_TRAIT_TRUE((std::is_same<Q_add_pointer_impl<void>::type, void*>));

#if !BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, <= 1800 )
    BOOST_TEST_TRAIT_TRUE((has_type<Q_add_pointer_impl<int>>));
#endif
    BOOST_TEST_TRAIT_TRUE((std::is_same<Q_add_pointer_impl<int>::type, int*>));

    BOOST_TEST_TRAIT_FALSE((has_type<Q_add_pointer_impl<>>));
    BOOST_TEST_TRAIT_FALSE((has_type<Q_add_pointer_impl<void, void>>));

    return boost::report_errors();
}
