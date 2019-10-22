/*
Copyright 2014-2015 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/align/alignment_of.hpp>
#include <boost/align/is_aligned.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>

template<std::size_t N>
struct A { };

template<std::size_t N>
void test(char* p, A<N>)
{
    BOOST_TEST(boost::alignment::is_aligned(p, N));
    BOOST_TEST(!boost::alignment::is_aligned(p + 1, N));
}

void test(char* p, A<1>)
{
    BOOST_TEST(boost::alignment::is_aligned(p, 1));
}

template<class T>
void test()
{
    T o;
    test(reinterpret_cast<char*>(&o),
        A<boost::alignment::alignment_of<T>::value>());
}

class X;

int main()
{
    test<bool>();
    test<char>();
    test<wchar_t>();
#if !defined(BOOST_NO_CXX11_CHAR16_T)
    test<char16_t>();
#endif
#if !defined(BOOST_NO_CXX11_CHAR32_T)
    test<char32_t>();
#endif
    test<short>();
    test<int>();
    test<long>();
#if !defined(BOOST_NO_LONG_LONG) && !defined(_MSC_VER)
    test<long long>();
#endif
    test<float>();
#if !defined(BOOST_MSVC)
    test<double>();
    test<long double>();
#endif
    test<void*>();
    test<char*>();
    test<int*>();
    test<X*>();
    test<void(*)()>();
#if !defined(_MSC_VER) || !defined(__clang__)
#if !defined(BOOST_MSVC)
    test<int X::*>();
    test<int(X::*)()>();
#endif
#endif

    return boost::report_errors();
}
