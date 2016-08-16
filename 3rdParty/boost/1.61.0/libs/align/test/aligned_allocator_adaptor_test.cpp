/*
(c) 2014 Glen Joseph Fernandes
<glenjofe -at- gmail.com>

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#include <boost/align/aligned_allocator_adaptor.hpp>
#include <boost/align/is_aligned.hpp>
#include <boost/core/lightweight_test.hpp>
#include <new>
#include <cstring>

template<class T>
class A {
public:
    typedef T value_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef void* void_pointer;
    typedef const void* const_void_pointer;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;
    typedef T& reference;
    typedef const T& const_reference;
    template<class U>
    struct rebind {
        typedef A<U> other;
    };
    A()
        : state() {
    }
    A(int value)
        : state(value) {
    }
    template<class U>
    A(const A<U>& other)
        : state(other.state) {
    }
    pointer allocate(size_type size, const_void_pointer = 0) {
        return static_cast<T*>(::operator new(sizeof(T) * size));
    }
    void deallocate(pointer ptr, size_type) {
        ::operator delete(ptr);
    }
    void construct(pointer ptr, const_reference value) {
        ::new(static_cast<void*>(ptr)) T(value);
    }
    void destroy(pointer ptr) {
        (void)ptr;
        ptr->~T();
    }
    int state;
};

template<class T1, class T2>
bool operator==(const A<T1>& a, const A<T2>& b)
{
    return a.state == b.state;
}

template<class T1, class T2>
bool operator!=(const A<T1>& a, const A<T2>& b)
{
    return !(a == b);
}

template<std::size_t Alignment>
void test_allocate()
{
    {
        boost::alignment::aligned_allocator_adaptor<A<int>,
            Alignment> a(5);
        int* p = a.allocate(1);
        BOOST_TEST(p != 0);
        BOOST_TEST(boost::alignment::is_aligned(p, Alignment));
        std::memset(p, 0, 1);
        a.deallocate(p, 1);
    }
    {
        boost::alignment::aligned_allocator_adaptor<A<int>,
            Alignment> a(5);
        int* p1 = a.allocate(1);
        int* p2 = a.allocate(1, p1);
        BOOST_TEST(p2 != 0);
        BOOST_TEST(boost::alignment::is_aligned(p2, Alignment));
        std::memset(p2, 0, 1);
        a.deallocate(p2, 1);
        a.deallocate(p1, 1);
    }
    {
        boost::alignment::aligned_allocator_adaptor<A<int>,
            Alignment> a(5);
        int* p = a.allocate(0);
        a.deallocate(p, 0);
    }
}

template<std::size_t Alignment>
void test_construct()
{
    boost::alignment::aligned_allocator_adaptor<A<int>,
        Alignment> a(5);
    int* p = a.allocate(1);
    a.construct(p, 1);
    BOOST_TEST(*p == 1);
    a.destroy(p);
    a.deallocate(p, 1);
}

template<std::size_t Alignment>
void test_constructor()
{
    {
        boost::alignment::aligned_allocator_adaptor<A<char>,
            Alignment> a1(5);
        boost::alignment::aligned_allocator_adaptor<A<int>,
            Alignment> a2(a1);
        BOOST_TEST(a2 == a1);
    }
    {
        A<int> a1(5);
        boost::alignment::aligned_allocator_adaptor<A<int>,
            Alignment> a2(a1);
        BOOST_TEST(a2.base() == a1);
    }
}

template<std::size_t Alignment>
void test_rebind()
{
    boost::alignment::aligned_allocator_adaptor<A<int>,
        Alignment> a1(5);
    typename boost::alignment::aligned_allocator_adaptor<A<int>,
        Alignment>::template rebind<int>::other a2(a1);
    BOOST_TEST(a2 == a1);
}

template<std::size_t Alignment>
void test()
{
    test_allocate<Alignment>();
    test_construct<Alignment>();
    test_constructor<Alignment>();
    test_rebind<Alignment>();
}

int main()
{
    test<1>();
    test<2>();
    test<4>();
    test<8>();
    test<16>();
    test<32>();
    test<64>();
    test<128>();

    return boost::report_errors();
}
