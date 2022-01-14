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
    A1() { }
};

#if !defined(BOOST_NO_CXX11_ALLOCATOR)
template<class T>
struct A2 {
    typedef T value_type;
    A2() { }
    template<class U, class V>
    void construct(U* p, const V& v) {
        ::new((void*)p) U(v + 1);
    }
};
#endif

int main()
{
    {
        A1<int> a;
        int i = 0;
        boost::allocator_construct(a, &i, 5);
        BOOST_TEST_EQ(i, 5);
    }
#if !defined(BOOST_NO_CXX11_ALLOCATOR)
    {
        A1<int> a;
        int i = 0;
        boost::allocator_construct(a, &i, 5);
        BOOST_TEST_EQ(i, 6);
    }
#endif
    return boost::report_errors();
}
