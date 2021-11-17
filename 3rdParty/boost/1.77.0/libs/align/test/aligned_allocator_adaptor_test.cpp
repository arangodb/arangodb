/*
Copyright 2014 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
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
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    template<class U>
    struct rebind {
        typedef A<U> other;
    };

    A(int state)
        : state_(state) { }

    template<class U>
    A(const A<U>& other)
        : state_(other.state()) { }

    T* allocate(std::size_t size, const void* = 0) {
        return static_cast<T*>(::operator new(sizeof(T) * size));
    }

    void deallocate(T* ptr, std::size_t) {
        ::operator delete(ptr);
    }

    void construct(T* ptr, const T& value) {
        ::new(static_cast<void*>(ptr)) T(value);
    }

    void destroy(T* ptr) {
        ptr->~T();
    }

    int state() const {
        return state_;
    }

private:
    int state_;
};

template<class T, class U>
inline bool
operator==(const A<T>& a, const A<U>& b)
{
    return a.state() == b.state();
}

template<class T, class U>
inline bool
operator!=(const A<T>& a, const A<U>& b)
{
    return !(a == b);
}

template<std::size_t Alignment>
void test_allocate()
{
    {
        boost::alignment::aligned_allocator_adaptor<A<int>, Alignment> a(5);
        int* p = a.allocate(1);
        BOOST_TEST(p != 0);
        BOOST_TEST(boost::alignment::is_aligned(p, Alignment));
        std::memset(p, 0, 1);
        a.deallocate(p, 1);
    }
    {
        boost::alignment::aligned_allocator_adaptor<A<int>, Alignment> a(5);
        int* p = a.allocate(1);
        int* q = a.allocate(1, p);
        BOOST_TEST(q != 0);
        BOOST_TEST(boost::alignment::is_aligned(q, Alignment));
        std::memset(q, 0, 1);
        a.deallocate(q, 1);
        a.deallocate(p, 1);
    }
    {
        boost::alignment::aligned_allocator_adaptor<A<int>, Alignment> a(5);
        int* p = a.allocate(0);
        a.deallocate(p, 0);
    }
}

template<std::size_t Alignment>
void test_construct()
{
    boost::alignment::aligned_allocator_adaptor<A<int>, Alignment> a(5);
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
        boost::alignment::aligned_allocator_adaptor<A<char>, Alignment> a(5);
        boost::alignment::aligned_allocator_adaptor<A<int>, Alignment> b(a);
        BOOST_TEST(b == a);
    }
    {
        A<int> a(5);
        boost::alignment::aligned_allocator_adaptor<A<int>, Alignment> b(a);
        BOOST_TEST(b.base() == a);
    }
}

template<std::size_t Alignment>
void test_rebind()
{
    boost::alignment::aligned_allocator_adaptor<A<int>, Alignment> a(5);
    typename boost::alignment::aligned_allocator_adaptor<A<int>,
        Alignment>::template rebind<int>::other b(a);
    BOOST_TEST(b == a);
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
