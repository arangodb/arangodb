/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#ifdef __cpp_deduction_guides
#include <boost/core/span.hpp>
#include <boost/core/lightweight_test.hpp>

template<class T>
class range {
public:
    T* data() {
        return &v_[0];
    }

    std::size_t size() const {
        return 4;
    }

private:
    T v_[4];
};

void test_data_size()
{
    int a[4];
    boost::span s(&a[0], 4);
    BOOST_TEST_EQ(s.extent, boost::dynamic_extent);
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 4);
}

void test_first_last()
{
    int a[4];
    boost::span s(&a[0], &a[4]);
    BOOST_TEST_EQ(s.extent, boost::dynamic_extent);
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 4);
}

void test_array()
{
    int a[4];
    boost::span s(a);
    BOOST_TEST_EQ(s.extent, 4);
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 4);
}

void test_std_array()
{
    std::array<int, 4> a;
    boost::span s(a);
    BOOST_TEST_EQ(s.extent, 4);
    BOOST_TEST_EQ(s.data(), a.data());
    BOOST_TEST_EQ(s.size(), a.size());
}

void test_const_std_array()
{
    const std::array<int, 4> a = std::array<int, 4>();
    boost::span s(a);
    BOOST_TEST_EQ(s.extent, 4);
    BOOST_TEST_EQ(s.data(), a.data());
    BOOST_TEST_EQ(s.size(), a.size());
}

void test_range()
{
    range<int> c;
    boost::span s(c);
    BOOST_TEST_EQ(s.extent, boost::dynamic_extent);
    BOOST_TEST_EQ(s.data(), c.data());
    BOOST_TEST_EQ(s.size(), c.size());
}

void test_span_dynamic()
{
    int a[4];
    boost::span s(boost::span<int>(&a[0], 4));
    BOOST_TEST_EQ(s.extent, boost::dynamic_extent);
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 4);
}

void test_span_static()
{
    int a[4];
    boost::span s(boost::span<int, 4>(&a[0], 4));
    BOOST_TEST_EQ(s.extent, 4);
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 4);
}

int main()
{
    test_data_size();
    test_first_last();
    test_array();
    test_std_array();
    test_const_std_array();
    test_range();
    test_span_dynamic();
    test_span_static();
    return boost::report_errors();
}
#else
int main()
{
    return 0;
}
#endif
