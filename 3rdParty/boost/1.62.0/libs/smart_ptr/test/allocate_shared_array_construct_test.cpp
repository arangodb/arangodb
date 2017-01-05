/*
(c) 2012-2015 Glen Joseph Fernandes
<glenjofe -at- gmail.com>

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#include <boost/core/lightweight_test.hpp>
#include <boost/smart_ptr/make_shared.hpp>

struct tag { };

template<typename T>
class creator {
public:
    typedef T value_type;
    creator() {
    }
    template<typename U>
    creator(const creator<U>&) {
    }
    T* allocate(std::size_t size) {
        void* p = ::operator new(size * sizeof(T));
        return static_cast<T*>(p);
    }
    void deallocate(T* memory, std::size_t) {
        void* p = memory;
        ::operator delete(p);
    }
    template<typename U>
    void construct(U* memory) {
        void* p = memory;
        ::new(p) U(tag());
    }
    template<typename U>
    void destroy(U* memory) {
        memory->~U();
    }
};

class type {
public:
    static unsigned int instances;
    explicit type(tag) {
        instances++;
    }
    type(const type&) {
        instances++;
    }
    ~type() {
        instances--;
    }
};

unsigned int type::instances = 0;

int main()
{
#if !defined(BOOST_NO_CXX11_ALLOCATOR)
    {
        boost::shared_ptr<type[]> a1 = boost::allocate_shared<type[]>(creator<void>(), 3);
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 3);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<type[3]> a1 = boost::allocate_shared<type[3]>(creator<void>());
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 3);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<type[][2]> a1 = boost::allocate_shared<type[][2]>(creator<void>(), 2);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(type::instances == 4);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<type[2][2]> a1 = boost::allocate_shared<type[2][2]>(creator<void>());
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(type::instances == 4);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<const type[]> a1 = boost::allocate_shared<const type[]>(creator<void>(), 3);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(type::instances == 3);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<const type[3]> a1 = boost::allocate_shared<const type[3]>(creator<void>());
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(type::instances == 3);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<const type[][2]> a1 = boost::allocate_shared<const type[][2]>(creator<void>(), 2);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(type::instances == 4);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<const type[2][2]> a1 = boost::allocate_shared<const type[2][2]>(creator<void>());
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(type::instances == 4);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
#endif
    return boost::report_errors();
}
