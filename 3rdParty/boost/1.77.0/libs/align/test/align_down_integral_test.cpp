/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/align/align_down.hpp>
#include <boost/core/lightweight_test.hpp>

template<std::size_t N>
struct A { };

template<class T, std::size_t N>
void test(A<N>)
{
    T v1 = N;
    BOOST_TEST(boost::alignment::align_down(v1, N) == v1);
    T v2 = N + 1;
    BOOST_TEST(boost::alignment::align_down(v2, N) == v1);
}

template<class T>
void test(A<1>)
{
    T v = 1;
    BOOST_TEST(boost::alignment::align_down(v, 1) == v);
}

template<class T>
void test()
{
    test<T>(A<1>());
    test<T>(A<2>());
    test<T>(A<4>());
    test<T>(A<8>());
    test<T>(A<16>());
    test<T>(A<32>());
    test<T>(A<64>());
    test<T>(A<128>());
}

int main()
{
    test<int>();
    test<unsigned>();
    test<long>();
    test<unsigned long>();
    test<short>();
    test<unsigned short>();
#if !defined(BOOST_NO_LONG_LONG)
    test<long long>();
    test<unsigned long long>();
#endif
    test<std::size_t>();
    test<std::ptrdiff_t>();

    return boost::report_errors();
}
