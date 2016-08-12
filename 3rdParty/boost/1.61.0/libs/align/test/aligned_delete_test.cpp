/*
(c) 2014 Glen Joseph Fernandes
<glenjofe -at- gmail.com>

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#include <boost/align/aligned_alloc.hpp>
#include <boost/align/aligned_delete.hpp>
#include <boost/align/alignment_of.hpp>
#include <boost/core/lightweight_test.hpp>
#include <new>

template<class T>
class type {
public:
    static int count;
    type()
        : value() {
        count++;
    }
    ~type() {
        count--;
    }
private:
    T value;
};

template<class T>
int type<T>::count = 0;

template<class T>
T* aligned_new()
{
    void* p = boost::alignment::aligned_alloc(boost::
        alignment::alignment_of<T>::value, sizeof(T));
    if (p) {
        return ::new(p) T();
    } else {
        throw std::bad_alloc();
    }
}

template<class T>
void test()
{
    type<T>* p = aligned_new<type<T> >();
    BOOST_TEST(type<T>::count == 1);
    boost::alignment::aligned_delete()(p);
    BOOST_TEST(type<T>::count == 0);
}

class C { };
union U { };

int main()
{
    test<char>();
    test<bool>();
    test<short>();
    test<int>();
    test<long>();
    test<float>();
    test<double>();
    test<long double>();
    test<void*>();
    test<void(*)()>();
    test<C>();
    test<int C::*>();
    test<int (C::*)()>();
    test<U>();

    return boost::report_errors();
}
