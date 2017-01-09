/*
(c) 2014-2015 Glen Joseph Fernandes
<glenjofe -at- gmail.com>

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#include <boost/align/align.hpp>
#include <boost/align/is_aligned.hpp>
#include <boost/core/lightweight_test.hpp>

template<std::size_t Alignment>
void test()
{
    char s[Alignment << 1];
    char* b = s;
    while (!boost::alignment::is_aligned(b, Alignment)) {
        b++;
    }
    {
        std::size_t n = Alignment;
        void* p = b;
        void* q = boost::alignment::align(Alignment, 1, p, n);
        BOOST_TEST(q == p);
        BOOST_TEST(q == b);
        BOOST_TEST(boost::alignment::is_aligned(q, Alignment));
        BOOST_TEST(n == Alignment);
    }
    {
        std::size_t n = 0;
        void* p = b;
        void* q = boost::alignment::align(Alignment, 1, p, n);
        BOOST_TEST(q == 0);
        BOOST_TEST(p == b);
        BOOST_TEST(n == 0);
    }
    {
        std::size_t n = Alignment - 1;
        void* p = &b[1];
        void* q = boost::alignment::align(Alignment, 1, p, n);
        BOOST_TEST(q == 0);
        BOOST_TEST(p == &b[1]);
        BOOST_TEST(n == Alignment - 1);
    }
    {
        std::size_t n = Alignment;
        void* p = &b[1];
        void* q = boost::alignment::align(Alignment, 1, p, n);
        BOOST_TEST(q == p);
        BOOST_TEST(p == &b[Alignment]);
        BOOST_TEST(boost::alignment::is_aligned(q, Alignment));
        BOOST_TEST(n == 1);
    }
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
