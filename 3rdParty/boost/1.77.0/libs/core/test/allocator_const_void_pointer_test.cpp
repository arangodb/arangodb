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
    typedef int* const_pointer;
    typedef int* const_void_pointer;
    template<class U>
    struct rebind {
        typedef A1<U> other;
    };
};

#if !defined(BOOST_NO_CXX11_ALLOCATOR)
template<class T>
struct A2 {
    typedef T value_type;
};
#endif

int main()
{
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<int*,
        boost::allocator_const_void_pointer<A1<char> >::type>));
#if !defined(BOOST_NO_CXX11_ALLOCATOR)
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<const void*,
        boost::allocator_const_void_pointer<A2<int> >::type>));
#endif
    return boost::report_errors();
}
