/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/config.hpp>
#if (!defined(BOOST_LIBSTDCXX_VERSION) || \
    BOOST_LIBSTDCXX_VERSION >= 48000) && \
    !defined(BOOST_NO_CXX11_SMART_PTR)
#include <boost/smart_ptr/allocate_unique.hpp>
#include <boost/core/lightweight_test.hpp>

template<class T = void>
struct creator {
    typedef T value_type;
    typedef T* pointer;

    template<class U>
    struct rebind {
        typedef creator<U> other;
    };

    creator() { }

    template<class U>
    creator(const creator<U>&) { }

    T* allocate(std::size_t size) {
        return static_cast<T*>(::operator new(sizeof(T) * size));
    }

    void deallocate(T* ptr, std::size_t) {
        ::operator delete(ptr);
    }
};

template<class T, class U>
inline bool
operator==(const creator<T>&, const creator<U>&)
{
    return true;
}

template<class T, class U>
inline bool
operator!=(const creator<T>&, const creator<U>&)
{
    return false;
}

class type {
public:
    static unsigned instances;

    type()
        : value_(0.0) {
        ++instances;
    }

    ~type() {
        --instances;
    }

    void set(long double value) {
        value_ = value;
    }

    long double get() const {
        return value_;
    }

private:
    type(const type&);
    type& operator=(const type&);

    long double value_;
};

unsigned type::instances = 0;

int main()
{
    {
        std::unique_ptr<int[],
            boost::alloc_deleter<int[],
            boost::noinit_adaptor<creator<int> > > > result =
            boost::allocate_unique_noinit<int[]>(creator<int>(), 3);
        BOOST_TEST(result.get() != 0);
    }
    {
        std::unique_ptr<int[],
            boost::alloc_deleter<int[3],
            boost::noinit_adaptor<creator<int> > > > result =
            boost::allocate_unique_noinit<int[3]>(creator<int>());
        BOOST_TEST(result.get() != 0);
    }
    {
        std::unique_ptr<int[][2],
            boost::alloc_deleter<int[][2],
            boost::noinit_adaptor<creator<> > > > result =
            boost::allocate_unique_noinit<int[][2]>(creator<>(), 2);
        BOOST_TEST(result.get() != 0);
    }
    {
        std::unique_ptr<int[][2],
            boost::alloc_deleter<int[2][2],
            boost::noinit_adaptor<creator<> > > > result =
            boost::allocate_unique_noinit<int[2][2]>(creator<>());
        BOOST_TEST(result.get() != 0);
    }
    {
        std::unique_ptr<const int[],
            boost::alloc_deleter<const int[],
            boost::noinit_adaptor<creator<> > > > result =
            boost::allocate_unique_noinit<const int[]>(creator<>(), 3);
        BOOST_TEST(result.get() != 0);
    }
    {
        std::unique_ptr<const int[],
            boost::alloc_deleter<const int[3],
            boost::noinit_adaptor<creator<> > > > result =
            boost::allocate_unique_noinit<const int[3]>(creator<>());
        BOOST_TEST(result.get() != 0);
    }
    {
        std::unique_ptr<const int[][2],
            boost::alloc_deleter<const int[][2],
            boost::noinit_adaptor<creator<> > > > result =
            boost::allocate_unique_noinit<const int[][2]>(creator<>(), 2);
        BOOST_TEST(result.get() != 0);
    }
    {
        std::unique_ptr<const int[][2],
            boost::alloc_deleter<const int[2][2],
            boost::noinit_adaptor<creator<> > > > result =
            boost::allocate_unique_noinit<const int[2][2]>(creator<>());
        BOOST_TEST(result.get() != 0);
    }
    {
        std::unique_ptr<type[],
            boost::alloc_deleter<type[],
            boost::noinit_adaptor<creator<type> > > > result =
            boost::allocate_unique_noinit<type[]>(creator<type>(), 3);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(type::instances == 3);
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        std::unique_ptr<type[],
            boost::alloc_deleter<type[3],
            boost::noinit_adaptor<creator<type> > > > result =
            boost::allocate_unique_noinit<type[3]>(creator<type>());
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(type::instances == 3);
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        std::unique_ptr<type[][2],
            boost::alloc_deleter<type[][2],
            boost::noinit_adaptor<creator<> > > > result =
            boost::allocate_unique_noinit<type[][2]>(creator<>(), 2);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(type::instances == 4);
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        std::unique_ptr<type[][2],
            boost::alloc_deleter<type[2][2],
            boost::noinit_adaptor<creator<> > > > result =
            boost::allocate_unique_noinit<type[2][2]>(creator<>());
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(type::instances == 4);
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        std::unique_ptr<const type[],
            boost::alloc_deleter<const type[],
            boost::noinit_adaptor<creator<> > > > result =
            boost::allocate_unique_noinit<const type[]>(creator<>(), 3);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(type::instances == 3);
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        std::unique_ptr<const type[],
            boost::alloc_deleter<const type[3],
            boost::noinit_adaptor<creator<> > > > result =
            boost::allocate_unique_noinit<const type[3]>(creator<>());
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(type::instances == 3);
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        std::unique_ptr<const type[][2],
            boost::alloc_deleter<const type[][2],
            boost::noinit_adaptor<creator<> > > > result =
            boost::allocate_unique_noinit<const type[][2]>(creator<>(), 2);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(type::instances == 4);
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        std::unique_ptr<const type[][2],
            boost::alloc_deleter<const type[2][2],
            boost::noinit_adaptor<creator<> > > > result =
            boost::allocate_unique_noinit<const type[2][2]>(creator<>());
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(type::instances == 4);
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    return boost::report_errors();
}
#else
int main()
{
    return 0;
}
#endif
