/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/default_allocator.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <vector>
#include <list>

class type {
public:
    explicit type(double value)
        : value_(value) { }

private:
    type(const type&);
    type& operator=(const type&);

    double value_;
};

void test_value_type()
{
    BOOST_TEST_TRAIT_SAME(int,
        boost::default_allocator<int>::value_type);
    BOOST_TEST_TRAIT_SAME(type,
        boost::default_allocator<type>::value_type);
    BOOST_TEST_TRAIT_SAME(int[5],
        boost::default_allocator<int[5]>::value_type);
    BOOST_TEST_TRAIT_SAME(void,
        boost::default_allocator<void>::value_type);
}

void test_pointer()
{
    BOOST_TEST_TRAIT_SAME(int*,
        boost::default_allocator<int>::pointer);
    BOOST_TEST_TRAIT_SAME(type*,
        boost::default_allocator<type>::pointer);
    BOOST_TEST_TRAIT_SAME(int(*)[5],
        boost::default_allocator<int[5]>::pointer);
    BOOST_TEST_TRAIT_SAME(void*,
        boost::default_allocator<void>::pointer);
}

void test_const_pointer()
{
    BOOST_TEST_TRAIT_SAME(const int*,
        boost::default_allocator<int>::const_pointer);
    BOOST_TEST_TRAIT_SAME(const type*,
        boost::default_allocator<type>::const_pointer);
    BOOST_TEST_TRAIT_SAME(const int(*)[5],
        boost::default_allocator<int[5]>::const_pointer);
    BOOST_TEST_TRAIT_SAME(const void*,
        boost::default_allocator<void>::const_pointer);
}

void test_reference()
{
    BOOST_TEST_TRAIT_SAME(int&,
        boost::default_allocator<int>::reference);
    BOOST_TEST_TRAIT_SAME(type&,
        boost::default_allocator<type>::reference);
    BOOST_TEST_TRAIT_SAME(int(&)[5],
        boost::default_allocator<int[5]>::reference);
    BOOST_TEST_TRAIT_SAME(void,
        boost::default_allocator<void>::reference);
}

void test_const_reference()
{
    BOOST_TEST_TRAIT_SAME(const int&,
        boost::default_allocator<int>::const_reference);
    BOOST_TEST_TRAIT_SAME(const type&,
        boost::default_allocator<type>::const_reference);
    BOOST_TEST_TRAIT_SAME(const int(&)[5],
        boost::default_allocator<int[5]>::const_reference);
    BOOST_TEST_TRAIT_SAME(const void,
        boost::default_allocator<void>::const_reference);
}

void test_size_type()
{
    BOOST_TEST_TRAIT_SAME(std::size_t,
        boost::default_allocator<int>::size_type);
    BOOST_TEST_TRAIT_SAME(std::size_t,
        boost::default_allocator<type>::size_type);
    BOOST_TEST_TRAIT_SAME(std::size_t,
        boost::default_allocator<int[5]>::size_type);
    BOOST_TEST_TRAIT_SAME(std::size_t,
        boost::default_allocator<void>::size_type);
}

void test_difference_type()
{
    BOOST_TEST_TRAIT_SAME(std::ptrdiff_t,
        boost::default_allocator<int>::difference_type);
    BOOST_TEST_TRAIT_SAME(std::ptrdiff_t,
        boost::default_allocator<type>::difference_type);
    BOOST_TEST_TRAIT_SAME(std::ptrdiff_t,
        boost::default_allocator<int[5]>::difference_type);
    BOOST_TEST_TRAIT_SAME(std::ptrdiff_t,
        boost::default_allocator<void>::difference_type);
}

void test_propagate_on_container_move_assignment()
{
    BOOST_TEST_TRAIT_TRUE((boost::default_allocator<int>::
        propagate_on_container_move_assignment));
    BOOST_TEST_TRAIT_TRUE((boost::default_allocator<type>::
        propagate_on_container_move_assignment));
    BOOST_TEST_TRAIT_TRUE((boost::default_allocator<int[5]>::
        propagate_on_container_move_assignment));
    BOOST_TEST_TRAIT_TRUE((boost::default_allocator<void>::
        propagate_on_container_move_assignment));
}

void test_is_always_equal()
{
    BOOST_TEST_TRAIT_TRUE((boost::default_allocator<int>::is_always_equal));
    BOOST_TEST_TRAIT_TRUE((boost::default_allocator<type>::is_always_equal));
    BOOST_TEST_TRAIT_TRUE((boost::default_allocator<int[5]>::is_always_equal));
    BOOST_TEST_TRAIT_TRUE((boost::default_allocator<void>::is_always_equal));
}

void test_rebind()
{
    BOOST_TEST_TRAIT_SAME(boost::default_allocator<type>,
        boost::default_allocator<int>::rebind<type>::other);
    BOOST_TEST_TRAIT_SAME(boost::default_allocator<int[5]>,
        boost::default_allocator<type>::rebind<int[5]>::other);
    BOOST_TEST_TRAIT_SAME(boost::default_allocator<void>,
        boost::default_allocator<int[5]>::rebind<void>::other);
    BOOST_TEST_TRAIT_SAME(boost::default_allocator<int>,
        boost::default_allocator<void>::rebind<int>::other);
}

void test_default_construct()
{
    boost::default_allocator<int> a1;
    (void)a1;
    boost::default_allocator<type> a2;
    (void)a2;
    boost::default_allocator<int[5]> a3;
    (void)a3;
    boost::default_allocator<void> a4;
    (void)a4;
}

void test_copy()
{
    boost::default_allocator<int> a1;
    boost::default_allocator<int> a2(a1);
    (void)a2;
    boost::default_allocator<int[5]> a3;
    boost::default_allocator<int[5]> a4(a3);
    (void)a4;
    boost::default_allocator<void> a5;
    boost::default_allocator<void> a6(a5);
    (void)a6;
}

void test_construct_other()
{
    boost::default_allocator<int> a1;
    boost::default_allocator<type> a2(a1);
    boost::default_allocator<int[5]> a3(a2);
    boost::default_allocator<void> a4(a3);
    boost::default_allocator<int> a5(a4);
    (void)a5;
}

#if defined(PTRDIFF_MAX) && defined(SIZE_MAX)
template<class T>
std::size_t max_size()
{
    return PTRDIFF_MAX < SIZE_MAX / sizeof(T)
        ? PTRDIFF_MAX : SIZE_MAX / sizeof(T);
}
#else
template<class T>
std::size_t max_size()
{
    return ~static_cast<std::size_t>(0) / sizeof(T);
}
#endif

void test_max_size()
{
    BOOST_TEST_EQ(max_size<int>(),
        boost::default_allocator<int>().max_size());
    BOOST_TEST_EQ(max_size<type>(),
        boost::default_allocator<type>().max_size());
    BOOST_TEST_EQ(max_size<int[5]>(),
        boost::default_allocator<int[5]>().max_size());
}

template<class T>
void test_allocate()
{
    boost::default_allocator<T> a;
    T* p = a.allocate(1);
    BOOST_TEST(p != 0);
    a.deallocate(p, 1);
    p = a.allocate(0);
    a.deallocate(p, 0);
    BOOST_TEST_THROWS(a.allocate(a.max_size() + 1), std::bad_alloc);
}

void test_allocate_deallocate()
{
    test_allocate<int>();
    test_allocate<type>();
    test_allocate<int[5]>();
}

void test_equals()
{
    BOOST_TEST(boost::default_allocator<int>() ==
        boost::default_allocator<type>());
    BOOST_TEST(boost::default_allocator<type>() ==
        boost::default_allocator<int[5]>());
    BOOST_TEST(boost::default_allocator<int[5]>() ==
        boost::default_allocator<void>());
    BOOST_TEST(boost::default_allocator<void>() ==
        boost::default_allocator<int>());
}

void test_not_equals()
{
    BOOST_TEST(!(boost::default_allocator<int>() !=
        boost::default_allocator<type>()));
    BOOST_TEST(!(boost::default_allocator<type>() !=
        boost::default_allocator<int[5]>()));
    BOOST_TEST(!(boost::default_allocator<int[5]>() !=
        boost::default_allocator<void>()));
    BOOST_TEST(!(boost::default_allocator<void>() !=
        boost::default_allocator<int>()));
}

void test_container()
{
    std::vector<int, boost::default_allocator<int> > v;
    v.push_back(1);
    BOOST_TEST(v.size() == 1);
    BOOST_TEST(v.front() == 1);
    std::list<int, boost::default_allocator<int> > l;
    l.push_back(1);
    BOOST_TEST(l.size() == 1);
    BOOST_TEST(l.front() == 1);
}

int main()
{
    test_value_type();
    test_pointer();
    test_const_pointer();
    test_reference();
    test_const_reference();
    test_size_type();
    test_difference_type();
    test_propagate_on_container_move_assignment();
    test_is_always_equal();
    test_rebind();
    test_default_construct();
    test_copy();
    test_construct_other();
    test_max_size();
    test_allocate_deallocate();
    test_equals();
    test_not_equals();
    test_container();
    return boost::report_errors();
}
