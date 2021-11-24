/*
Copyright 2020 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/allocator_access.hpp>
#include <boost/core/is_same.hpp>
#include <boost/core/lightweight_test_trait.hpp>

template<class T>
struct A1 {
    typedef T value_type;
    int value;
};

#if !defined(BOOST_NO_CXX11_ALLOCATOR)
template<class T>
struct A2 {
    typedef T value_type;
};

template<class T>
struct A3 {
    typedef T value_type;
    typedef std::false_type is_always_equal;
};

template<class T>
struct A4 {
    typedef T value_type;
    typedef std::true_type is_always_equal;
    int value;
};
#endif

int main()
{
    BOOST_TEST_TRAIT_FALSE((boost::allocator_is_always_equal<A1<int> >::type));
 #if !defined(BOOST_NO_CXX11_ALLOCATOR)
    BOOST_TEST_TRAIT_TRUE((boost::allocator_is_always_equal<A2<int> >::type));
    BOOST_TEST_TRAIT_FALSE((boost::allocator_is_always_equal<A3<int> >::type));
    BOOST_TEST_TRAIT_TRUE((boost::allocator_is_always_equal<A4<int> >::type));
 #endif
    return boost::report_errors();
}
