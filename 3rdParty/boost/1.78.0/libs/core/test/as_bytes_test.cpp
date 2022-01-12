/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <cstddef>
#ifdef __cpp_lib_byte
#include <boost/core/span.hpp>
#include <boost/core/lightweight_test.hpp>

void test_dynamic()
{
    int a[4];
    boost::span<const std::byte> s =
        boost::as_bytes(boost::span<int>(&a[0], 4));
    BOOST_TEST_EQ(s.data(), reinterpret_cast<const std::byte*>(&a[0]));
    BOOST_TEST_EQ(s.size(), sizeof(int) * 4);
}

void test_static()
{
    int a[4];
    boost::span<const std::byte, sizeof(int) * 4> s =
        boost::as_bytes(boost::span<int, 4>(&a[0], 4));
    BOOST_TEST_EQ(s.data(), reinterpret_cast<const std::byte*>(&a[0]));
    BOOST_TEST_EQ(s.size(), sizeof(int) * 4);
}

int main()
{
    test_dynamic();
    test_static();
    return boost::report_errors();
}
#else
int main()
{
    return 0;
}
#endif
