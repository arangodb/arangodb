/*
Copyright 2014 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/align/aligned_allocator.hpp>
#include <boost/align/is_aligned.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstring>

template<std::size_t Alignment>
void test_allocate()
{
    {
        boost::alignment::aligned_allocator<int, Alignment> a;
        int* p = a.allocate(1);
        BOOST_TEST(p != 0);
        BOOST_TEST(boost::alignment::is_aligned(p, Alignment));
        std::memset(p, 0, 1);
        a.deallocate(p, 1);
    }
    {
        boost::alignment::aligned_allocator<int, Alignment> a;
        int* p = a.allocate(0);
        a.deallocate(p, 0);
    }
}

template<std::size_t Alignment>
void test_construct()
{
    boost::alignment::aligned_allocator<int, Alignment> a;
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
        boost::alignment::aligned_allocator<char, Alignment> a1;
        boost::alignment::aligned_allocator<int, Alignment> a2(a1);
        BOOST_TEST(a2 == a1);
    }
    {
        boost::alignment::aligned_allocator<char, Alignment> a1;
        boost::alignment::aligned_allocator<void, Alignment> a2(a1);
        BOOST_TEST(a2 == a1);
    }
    {
        boost::alignment::aligned_allocator<void, Alignment> a1;
        boost::alignment::aligned_allocator<char, Alignment> a2(a1);
        BOOST_TEST(a2 == a1);
    }
}

template<std::size_t Alignment>
void test_rebind()
{
    {
        boost::alignment::aligned_allocator<char, Alignment> a1;
        typename boost::alignment::aligned_allocator<char,
            Alignment>::template rebind<int>::other a2(a1);
        BOOST_TEST(a2 == a1);
    }
    {
        boost::alignment::aligned_allocator<char, Alignment> a1;
        typename boost::alignment::aligned_allocator<char,
            Alignment>::template rebind<void>::other a2(a1);
        BOOST_TEST(a2 == a1);
    }
    {
        boost::alignment::aligned_allocator<void, Alignment> a1;
        typename boost::alignment::aligned_allocator<void,
            Alignment>::template rebind<char>::other a2(a1);
        BOOST_TEST(a2 == a1);
    }
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
