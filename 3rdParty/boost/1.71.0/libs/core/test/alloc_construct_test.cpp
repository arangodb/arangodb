/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/alloc_construct.hpp>
#include <boost/core/default_allocator.hpp>
#include <boost/core/lightweight_test.hpp>

class type {
public:
    explicit type(int x = 0, int y = 0)
        : value_(x + y) {
        ++count;
    }

    type(const type& other)
        : value_(other.value_) {
        ++count;
    }

    ~type() {
        --count;
    }

    int value() const {
        return value_;
    }

    static int count;

private:
    int value_;
};

int type::count = 0;

void test_construct()
{
    boost::default_allocator<type> a;
    type* p = a.allocate(1);
    boost::alloc_construct(a, p);
    BOOST_TEST_EQ(type::count, 1);
    BOOST_TEST_EQ(p->value(), 0);
    boost::alloc_destroy(a, p);
    BOOST_TEST_EQ(type::count, 0);
    a.deallocate(p, 1);
}

void test_construct_value()
{
    boost::default_allocator<type> a;
    type* p = a.allocate(1);
    boost::alloc_construct(a, p, 1);
    BOOST_TEST_EQ(type::count, 1);
    BOOST_TEST_EQ(p->value(), 1);
    boost::alloc_destroy(a, p);
    BOOST_TEST_EQ(type::count, 0);
    a.deallocate(p, 1);
}

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && \
    !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
void test_construct_args()
{
    boost::default_allocator<type> a;
    type* p = a.allocate(1);
    boost::alloc_construct(a, p, 1, 2);
    BOOST_TEST_EQ(type::count, 1);
    BOOST_TEST_EQ(p->value(), 3);
    boost::alloc_destroy(a, p);
    BOOST_TEST_EQ(type::count, 0);
    a.deallocate(p, 1);
}
#endif

void test_construct_n()
{
    boost::default_allocator<type> a;
    type* p = a.allocate(3);
    boost::alloc_construct_n(a, p, 3);
    BOOST_TEST_EQ(type::count, 3);
    BOOST_TEST_EQ(p[0].value(), 0);
    BOOST_TEST_EQ(p[1].value(), 0);
    BOOST_TEST_EQ(p[2].value(), 0);
    boost::alloc_destroy_n(a, p, 3);
    BOOST_TEST_EQ(type::count, 0);
    a.deallocate(p, 3);
}

void test_construct_n_list()
{
    boost::default_allocator<type> a;
    type* p = a.allocate(3);
    type q(1);
    boost::alloc_construct_n(a, p, 3, &q, 1);
    BOOST_TEST_EQ(type::count, 4);
    BOOST_TEST_EQ(p[0].value(), 1);
    BOOST_TEST_EQ(p[1].value(), 1);
    BOOST_TEST_EQ(p[2].value(), 1);
    boost::alloc_destroy_n(a, p, 3);
    BOOST_TEST_EQ(type::count, 1);
    a.deallocate(p, 3);
}

void test_construct_n_iterator()
{
    boost::default_allocator<type> a;
    type* p = a.allocate(3);
    type l[] = { type(1), type(2), type(3) };
    boost::alloc_construct_n(a, p, 3, &l[0]);
    BOOST_TEST_EQ(type::count, 6);
    BOOST_TEST_EQ(p[0].value(), 1);
    BOOST_TEST_EQ(p[1].value(), 2);
    BOOST_TEST_EQ(p[2].value(), 3);
    boost::alloc_destroy_n(a, p, 3);
    BOOST_TEST_EQ(type::count, 3);
    a.deallocate(p, 3);
}

int main()
{
    test_construct();
    test_construct_value();
#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && \
    !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
    test_construct_args();
#endif
    test_construct_n();
    test_construct_n_list();
    test_construct_n_iterator();
    return boost::report_errors();
}
