/*
Copyright 2020 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/config.hpp>
#if !defined(BOOST_NO_CXX11_ALLOCATOR)
#include <boost/numeric/ublas/matrix.hpp>
#include <new>

template<class T>
struct Allocator {
    typedef T value_type;

    Allocator() BOOST_NOEXCEPT { }

    template<class U>
    Allocator(const Allocator<U>&) BOOST_NOEXCEPT { }

    T* allocate(std::size_t size) {
        return static_cast<T*>(::operator new(sizeof(T) * size));
    }

    void deallocate(T* ptr, std::size_t) {
        ::operator delete(ptr);
    }
};

template<class T, class U>
bool operator==(const Allocator<T>&, const Allocator<U>&) BOOST_NOEXCEPT
{
    return true;
}

template<class T, class U>
bool operator!=(const Allocator<T>&, const Allocator<U>&) BOOST_NOEXCEPT
{
    return false;
}

int main()
{
    boost::numeric::ublas::matrix<int,
        boost::numeric::ublas::row_major,
        boost::numeric::ublas::unbounded_array<int,
        Allocator<int> > > matrix(4, 4);
    matrix(1, 2) = 3;
}
#endif
