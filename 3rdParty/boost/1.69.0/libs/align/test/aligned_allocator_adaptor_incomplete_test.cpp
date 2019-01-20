/*
Copyright 2018 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/align/aligned_allocator_adaptor.hpp>

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

struct S;
struct V { };

void value_test()
{
    boost::alignment::aligned_allocator_adaptor<A<S> > a(A<S>(1));
    (void)a;
}

void rebind_test()
{
    boost::alignment::aligned_allocator_adaptor<A<V> > a(A<V>(1));
    boost::alignment::aligned_allocator_adaptor<A<V> >::rebind<S>::other r(a);
    (void)r;
}
