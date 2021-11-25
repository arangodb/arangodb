/*
Copyright 2020 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/allocator_access.hpp>
#include <boost/core/lightweight_test.hpp>

struct S {
    static int count;
    S() {
        ++count;
    }
    S(const S&) {
        ++count;
    }
    ~S() {
        --count;
    }
};

int S::count = 0;

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
    template<class U>
    void destroy(U* p) {
        *p = U();
    }
};
#endif

int main()
{
    {
        A1<int> a;
        S s;
        boost::allocator_destroy(a, &s);
        BOOST_TEST_EQ(S::count, 0);
        ::new((void*)&s) S();
    }
#if !defined(BOOST_NO_CXX11_ALLOCATOR)
    {
        A1<int> a;
        int i = 5;
        boost::allocator_destroy(a, &i);
        BOOST_TEST_EQ(i, 0);
    }
#endif
    return boost::report_errors();
}
