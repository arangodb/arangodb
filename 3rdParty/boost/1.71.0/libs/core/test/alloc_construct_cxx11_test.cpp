/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/config.hpp>
#if !defined(BOOST_NO_CXX11_ALLOCATOR)
#include <boost/core/alloc_construct.hpp>
#include <boost/core/lightweight_test.hpp>

class type {
public:
    explicit type(int x)
        : value_(x) { }

    int value() const {
        return value_;
    }

    static int count;

private:
    type(const type&);
    type& operator=(const type&);

    int value_;
};

int type::count = 0;

template<class T>
struct creator {
    typedef T value_type;

    creator() { }

    template<class U>
    creator(const creator<U>&) { }

    T* allocate(std::size_t size) {
        return static_cast<T*>(::operator new(sizeof(T) * size));
    }

    void deallocate(T* ptr, std::size_t) {
        ::operator delete(ptr);
    }

    template<class V>
    void construct(type* ptr, const V& value) {
        ::new(static_cast<void*>(ptr)) type(value + 1);
        ++type::count;
    }

    void destroy(type* ptr) {
        ptr->~type();
        --type::count;
    }
};

int main()
{
    creator<type> a;
    type* p = a.allocate(1);
    boost::alloc_construct(a, p, 1);
    BOOST_TEST_EQ(type::count, 1);
    BOOST_TEST_EQ(p->value(), 2);
    boost::alloc_destroy(a, p);
    BOOST_TEST_EQ(type::count, 0);
    return boost::report_errors();
}
#else
int main()
{
    return 0;
}
#endif
