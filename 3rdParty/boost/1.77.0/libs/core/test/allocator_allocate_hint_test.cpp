/*
Copyright 2020 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/allocator_access.hpp>
#include <boost/core/lightweight_test.hpp>

template<class T>
struct A1 {
    typedef T value_type;
    typedef std::size_t size_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    template<class U>
    struct rebind {
        typedef A1<U> other;
    };
    A1()
        : value() { }
    T* allocate(std::size_t n, const void*) {
        value = n;
        return 0;
    }
    std::size_t value;
};

#if !defined(BOOST_NO_CXX11_ALLOCATOR)
template<class T>
struct A2 {
    typedef T value_type;
    A2()
        : value() { }
    T* allocate(std::size_t n) {
        value = n;
        return 0;
    }
    std::size_t value;
};
#endif

int main()
{
    {
        A1<int> a;
        BOOST_TEST_NOT(boost::allocator_allocate(a, 5, 0));
        BOOST_TEST_EQ(a.value, 5);
    }
#if !defined(BOOST_NO_CXX11_ALLOCATOR)
    {
        A2<int> a;
        BOOST_TEST_NOT(boost::allocator_allocate(a, 5, 0));
        BOOST_TEST_EQ(a.value, 5);
    }
#endif
    return boost::report_errors();
}
