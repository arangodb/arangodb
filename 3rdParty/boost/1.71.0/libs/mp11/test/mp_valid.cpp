
// Copyright 2015 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/utility.hpp>
#include <boost/mp11/detail/config.hpp>
#include <boost/core/lightweight_test_trait.hpp>

template<class T> struct Xi
{
};

template<> struct Xi<void>
{
    using type = void;
};

template<class T> using X = typename Xi<T>::type;

template<class T> using add_pointer = T*;
template<class T> using add_reference = T&;
template<class T> using add_extents = T[];

using boost::mp11::mp_quote;

using QX = mp_quote<X>;
using Q_add_pointer = mp_quote<add_pointer>;

int main()
{
    using boost::mp11::mp_valid;
    using boost::mp11::mp_valid_q;
    using boost::mp11::mp_identity;

    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_identity>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<mp_identity, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<mp_identity, void, void>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<X>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<X, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<X, int>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<X, void, void>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<QX::fn>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<QX::fn, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<QX::fn, int>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<QX::fn, void, void>));

    BOOST_TEST_TRAIT_FALSE((mp_valid_q<QX>));
    BOOST_TEST_TRAIT_TRUE((mp_valid_q<QX, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid_q<QX, int>));
    BOOST_TEST_TRAIT_FALSE((mp_valid_q<QX, void, void>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<add_pointer>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<add_pointer, void>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<add_pointer, int>));
#if !BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, <= 1800 )
    // msvc-12.0 can form pointer to reference
    BOOST_TEST_TRAIT_FALSE((mp_valid<add_pointer, int&>));
#endif
    BOOST_TEST_TRAIT_FALSE((mp_valid<add_pointer, void, void>));

    BOOST_TEST_TRAIT_FALSE((mp_valid<Q_add_pointer::fn>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<Q_add_pointer::fn, void>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<Q_add_pointer::fn, int>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<Q_add_pointer::fn, void, void>));

    BOOST_TEST_TRAIT_FALSE((mp_valid_q<Q_add_pointer>));
    BOOST_TEST_TRAIT_TRUE((mp_valid_q<Q_add_pointer, void>));
    BOOST_TEST_TRAIT_TRUE((mp_valid_q<Q_add_pointer, int>));
    BOOST_TEST_TRAIT_FALSE((mp_valid_q<Q_add_pointer, void, void>));

#if !BOOST_MP11_WORKAROUND( BOOST_MP11_GCC, < 70000 )
    // g++ up to at least 6.3 doesn't like add_reference for some reason or other
    BOOST_TEST_TRAIT_FALSE((mp_valid<add_reference>));
#if !BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, <= 1800 )
    // msvc-12.0 gives an internal error here
    BOOST_TEST_TRAIT_FALSE((mp_valid<add_reference, void>));
#endif
    BOOST_TEST_TRAIT_TRUE((mp_valid<add_reference, int>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<add_reference, int, int>));
#endif

    BOOST_TEST_TRAIT_FALSE((mp_valid<add_extents>));
    BOOST_TEST_TRAIT_TRUE((mp_valid<add_extents, int>));
#if !BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, <= 1800 )
    // msvc-12.0 can form arrays to void or int&
    BOOST_TEST_TRAIT_FALSE((mp_valid<add_extents, void>));
    BOOST_TEST_TRAIT_FALSE((mp_valid<add_extents, int&>));
#endif
    BOOST_TEST_TRAIT_FALSE((mp_valid<add_extents, int, int>));

    return boost::report_errors();
}
