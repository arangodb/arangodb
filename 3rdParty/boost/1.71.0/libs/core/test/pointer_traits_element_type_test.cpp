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

template<class T>
struct E1 {
    typedef bool element_type;
};

template<class T1, class T2>
struct E2 {
    typedef bool element_type;
};

template<class T1, class T2, class T3>
struct E3 {
    typedef bool element_type;
};

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
template<class T, class... U>
struct P { };

template<class T, class... U>
struct E {
    typedef bool element_type;
};
#endif

int main()
{
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<int,
        boost::pointer_traits<int*>::element_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<int,
        boost::pointer_traits<P1<int> >::element_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<int,
        boost::pointer_traits<P2<int, char> >::element_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<int,
        boost::pointer_traits<P3<int, char, char> >::element_type>));
#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<int,
        boost::pointer_traits<P<int, char, char, char> >::element_type>));
#endif
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<bool,
        boost::pointer_traits<E1<int> >::element_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<bool,
        boost::pointer_traits<E2<int, int> >::element_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<bool,
        boost::pointer_traits<E3<int, int, int> >::element_type>));
#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<bool,
        boost::pointer_traits<E<int, int, int, int> >::element_type>));
#endif
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<void,
        boost::pointer_traits<void*>::element_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<void,
        boost::pointer_traits<P1<void> >::element_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<bool,
        boost::pointer_traits<E1<void> >::element_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<const int,
        boost::pointer_traits<const int*>::element_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<const int,
        boost::pointer_traits<P1<const int> >::element_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<bool,
        boost::pointer_traits<E1<const int> >::element_type>));
    return boost::report_errors();
}
