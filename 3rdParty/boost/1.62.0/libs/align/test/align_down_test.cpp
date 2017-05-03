/*
(c) 2015 Glen Joseph Fernandes
<glenjofe -at- gmail.com>

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#include <boost/align/align_down.hpp>
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
        void* p = &b[Alignment];
        BOOST_TEST(boost::alignment::align_down(p, Alignment) == p);
    }
    {
        void* p = &b[Alignment - 1];
        void* q = b;
        BOOST_TEST(boost::alignment::align_down(p, Alignment) == q);
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
