/*
Copyright 2017 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/pointer_traits.hpp>
#include <boost/core/is_same.hpp>
#include <boost/core/lightweight_test_trait.hpp>

template<class T>
struct P1 { };

template<class T1, class T2>
struct P2 { };

template<class T1, class T2, class T3>
struct P3 { };

#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
template<class T>
struct E1 {
    template<class U>
    using rebind = E1<bool>;
};

template<class T1, class T2>
struct E2 {
    template<class U>
    using rebind = E2<bool, T2>;
};

template<class T1, class T2, class T3>
struct E3 {
    template<class U>
    using rebind = E3<bool, T2, T3>;
};
#endif

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
template<class T, class... U>
struct P { };

#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
template<class T, class... U>
struct E {
    template<class V>
    using rebind = E<bool, U...>;
};
#endif
#endif

struct R { };

int main()
{
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<char*,
        boost::pointer_traits<R*>::rebind_to<char>::type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<P1<char>,
        boost::pointer_traits<P1<R> >::rebind_to<char>::type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<P2<char, R>,
        boost::pointer_traits<P2<R, R> >::rebind_to<char>::type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<P3<char, R, R>,
        boost::pointer_traits<P3<R, R, R> >::rebind_to<char>::type>));
#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<P<char, R, R, R>,
        boost::pointer_traits<P<R, R, R, R> >::rebind_to<char>::type>));
#endif
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<void*,
        boost::pointer_traits<R*>::rebind_to<void>::type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<P1<void>,
        boost::pointer_traits<P1<R> >::rebind_to<void>::type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<R*,
        boost::pointer_traits<void*>::rebind_to<R>::type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<P1<R>,
        boost::pointer_traits<P1<void> >::rebind_to<R>::type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<const int*,
        boost::pointer_traits<R*>::rebind_to<const int>::type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<P1<const int>,
        boost::pointer_traits<P1<R> >::rebind_to<const int>::type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<int*,
        boost::pointer_traits<const R*>::rebind_to<int>::type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<P1<int>,
        boost::pointer_traits<P1<const R> >::rebind_to<int>::type>));
#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<E1<bool>,
        boost::pointer_traits<E1<R> >::rebind_to<char>::type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<E2<bool, R>,
        boost::pointer_traits<E2<R, R> >::rebind_to<char>::type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<E3<bool, R, R>,
        boost::pointer_traits<E3<R, R, R> >::rebind_to<char>::type>));
#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<E<bool, R, R, R>,
        boost::pointer_traits<E<R, R, R, R> >::rebind_to<char>::type>));
#endif
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<E1<bool>,
        boost::pointer_traits<E1<R> >::rebind_to<void>::type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<E1<bool>,
        boost::pointer_traits<E1<void> >::rebind_to<R>::type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<E1<bool>,
        boost::pointer_traits<E1<R> >::rebind_to<const int>::type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<E1<bool>,
        boost::pointer_traits<E1<const R> >::rebind_to<int>::type>));
#endif
    return boost::report_errors();
}
