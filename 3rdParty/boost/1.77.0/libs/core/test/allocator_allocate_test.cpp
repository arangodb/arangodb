/*
Copyright 2020 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/allocator_access.hpp>
#include <boost/core/lightweight_test.hpp>

template<class T>
struct A {
    typedef T value_type;
    typedef T* pointer;
    typedef std::size_t size_type;
    A()
        : value() { }
    T* allocate(std::size_t n) {
        value = n;
        return 0;
    }
    std::size_t value;
};

int main()
{
    A<int> a;
    BOOST_TEST_NOT(boost::allocator_allocate(a, 5));
    BOOST_TEST_EQ(a.value, 5);
    return boost::report_errors();
}
