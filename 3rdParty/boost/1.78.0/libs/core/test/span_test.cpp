/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/config.hpp>
#if !defined(BOOST_NO_CXX11_CONSTEXPR) && !defined(BOOST_NO_CXX11_DECLTYPE)
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

void test_extent()
{
    BOOST_TEST_EQ(boost::span<int>::extent,
        boost::dynamic_extent);
    BOOST_TEST_EQ((boost::span<int, 2>::extent), 2);
}

void test_default_construct_dynamic()
{
    boost::span<int> s;
    BOOST_TEST_EQ(s.data(), static_cast<int*>(0));
    BOOST_TEST_EQ(s.size(), 0);
}

void test_default_construct_static()
{
    boost::span<int, 0> s;
    BOOST_TEST_EQ(s.data(), static_cast<int*>(0));
    BOOST_TEST_EQ(s.size(), 0);
}

void test_construct_data_size()
{
    int a[4];
    boost::span<int> s(&a[0], 4);
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 4);
}

void test_construct_first_last()
{
    int a[4];
    boost::span<int> s(&a[0], &a[4]);
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 4);
}

void test_construct_array_dynamic()
{
    int a[4];
    boost::span<int> s(a);
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 4);
}

void test_construct_array_static()
{
    int a[4];
    boost::span<int, 4> s(a);
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 4);
}

void test_construct_std_array_dynamic()
{
    std::array<int, 4> a;
    boost::span<int> s(a);
    BOOST_TEST_EQ(s.data(), a.data());
    BOOST_TEST_EQ(s.size(), a.size());
}

void test_construct_std_array_static()
{
    std::array<int, 4> a;
    boost::span<int, 4> s(a);
    BOOST_TEST_EQ(s.data(), a.data());
    BOOST_TEST_EQ(s.size(), a.size());
}

void test_construct_const_std_array_dynamic()
{
    const std::array<int, 4> a = std::array<int, 4>();
    boost::span<const int> s(a);
    BOOST_TEST_EQ(s.data(), a.data());
    BOOST_TEST_EQ(s.size(), a.size());
}

void test_construct_const_std_array_static()
{
    const std::array<int, 4> a = std::array<int, 4>();
    boost::span<const int, 4> s(a);
    BOOST_TEST_EQ(s.data(), a.data());
    BOOST_TEST_EQ(s.size(), a.size());
}

void test_construct_range()
{
    range<int> c;
    boost::span<int> s(c);
    BOOST_TEST_EQ(s.data(), c.data());
    BOOST_TEST_EQ(s.size(), c.size());
}

void test_construct_span_dynamic()
{
    int a[4];
    boost::span<const int> s(boost::span<int>(&a[0], 4));
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 4);
}

void test_construct_span_dynamic_static()
{
    int a[4];
    boost::span<int> s(boost::span<int, 4>(&a[0], 4));
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 4);
}

void test_construct_span_static()
{
    int a[4];
    boost::span<const int, 4> s(boost::span<int, 4>(&a[0], 4));
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 4);
}

void test_construct_span_static_dynamic()
{
    int a[4];
    boost::span<int, 4> s(boost::span<int>(&a[0], 4));
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 4);
}

void test_copy_dynamic()
{
    int a[4];
    boost::span<int> s1(&a[0], 4);
    boost::span<int> s2(s1);
    BOOST_TEST_EQ(s2.data(), &a[0]);
    BOOST_TEST_EQ(s2.size(), 4);
}

void test_copy_static()
{
    int a[4];
    boost::span<int, 4> s1(&a[0], 4);
    boost::span<int, 4> s2(s1);
    BOOST_TEST_EQ(s2.data(), &a[0]);
    BOOST_TEST_EQ(s2.size(), 4);
}

void test_assign_dynamic()
{
    boost::span<int> s;
    int a[4];
    s = boost::span<int>(&a[0], 4);
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 4);
}

void test_assign_static()
{
    int a1[4];
    boost::span<int, 4> s(&a1[0], 4);
    int a2[4];
    s = boost::span<int, 4>(&a2[0], 4);
    BOOST_TEST_EQ(s.data(), &a2[0]);
    BOOST_TEST_EQ(s.size(), 4);
}

void test_first()
{
    int a[4];
    boost::span<int, 2> s = boost::span<int>(&a[0],
        4).first<2>();
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 2);
}

void test_last()
{
    int a[4];
    boost::span<int, 2> s = boost::span<int>(&a[0], 4).last<2>();
    BOOST_TEST_EQ(s.data(), &a[2]);
    BOOST_TEST_EQ(s.size(), 2);
}

void test_subspan_dynamic()
{
    int a[4];
    boost::span<int> s = boost::span<int>(&a[0], 4).subspan<2>();
    BOOST_TEST_EQ(s.data(), &a[2]);
    BOOST_TEST_EQ(s.size(), 2);
}

void test_subspan_static()
{
    int a[4];
    boost::span<int, 2> s = boost::span<int, 4>(&a[0],
        4).subspan<2>();
    BOOST_TEST_EQ(s.data(), &a[2]);
    BOOST_TEST_EQ(s.size(), 2);
}

void test_subspan()
{
    int a[4];
    boost::span<int, 1> s = boost::span<int>(&a[0],
        4).subspan<2, 1>();
    BOOST_TEST_EQ(s.data(), &a[2]);
    BOOST_TEST_EQ(s.size(), 1);
}

void test_first_size()
{
    int a[4];
    boost::span<int> s = boost::span<int>(&a[0], 4).first(2);
    BOOST_TEST_EQ(s.data(), &a[0]);
    BOOST_TEST_EQ(s.size(), 2);
}

void test_last_size()
{
    int a[4];
    boost::span<int> s = boost::span<int>(&a[0], 4).last(2);
    BOOST_TEST_EQ(s.data(), &a[2]);
    BOOST_TEST_EQ(s.size(), 2);
}

void test_subspan_range()
{
    int a[4];
    boost::span<int> s = boost::span<int>(&a[0], 4).subspan(2);
    BOOST_TEST_EQ(s.data(), &a[2]);
    BOOST_TEST_EQ(s.size(), 2);
}

void test_subspan_range_count()
{
    int a[4];
    boost::span<int> s = boost::span<int>(&a[0],
        4).subspan(2, 1);
    BOOST_TEST_EQ(s.data(), &a[2]);
    BOOST_TEST_EQ(s.size(), 1);
}

void test_size()
{
    int a[4];
    BOOST_TEST_EQ((boost::span<int>(&a[0], 4).size()), 4);
}

void test_size_bytes()
{
    int a[4];
    BOOST_TEST_EQ((boost::span<int>(&a[0], 4).size_bytes()),
        4 * sizeof(int));
}

void test_empty_dynamic()
{
    int a[4];
    BOOST_TEST(boost::span<int>().empty());
    BOOST_TEST_NOT((boost::span<int>(&a[0], 4).empty()));
}

void test_empty_static()
{
    int a[4];
    BOOST_TEST((boost::span<int, 0>().empty()));
    BOOST_TEST_NOT((boost::span<int, 4>(&a[0], 4).empty()));
}

void test_index()
{
    int a[4] = { 1, 2, 3, 4 };
    BOOST_TEST_EQ((boost::span<int>(&a[0], 4)[2]), 3);
}

void test_front()
{
    int a[4] = { 1, 2, 3, 4 };
    BOOST_TEST_EQ((boost::span<int>(&a[0], 4).front()), 1);
}

void test_back()
{
    int a[4] = { 1, 2, 3, 4 };
    BOOST_TEST_EQ((boost::span<int>(&a[0], 4).back()), 4);
}

void test_data()
{
    int a[4];
    BOOST_TEST_EQ((boost::span<int>(&a[0], 4).data()), &a[0]);
}

void test_begin()
{
    int a[4];
    BOOST_TEST_EQ((boost::span<int>(&a[0], 4).begin()), &a[0]);
}

void test_end()
{
    int a[4];
    BOOST_TEST_EQ((boost::span<int>(&a[0], 4).end()), &a[4]);
}

void test_rbegin()
{
    int a[4];
    BOOST_TEST_EQ((boost::span<int>(&a[0], 4).rbegin().base()), &a[4]);
}

void test_rend()
{
    int a[4];
    BOOST_TEST_EQ((boost::span<int>(&a[0], 4).rend().base()), &a[0]);
}

void test_cbegin()
{
    int a[4];
    BOOST_TEST_EQ((boost::span<int>(&a[0], 4).cbegin()), &a[0]);
}

void test_cend()
{
    int a[4];
    BOOST_TEST_EQ((boost::span<int>(&a[0], 4).cend()), &a[4]);
}

void test_crbegin()
{
    int a[4];
    BOOST_TEST_EQ((boost::span<int>(&a[0], 4).crbegin().base()), &a[4]);
}

void test_crend()
{
    int a[4];
    BOOST_TEST_EQ((boost::span<int>(&a[0], 4).crend().base()), &a[0]);
}

void test_begin_span()
{
    int a[4];
    BOOST_TEST_EQ((begin(boost::span<int>(&a[0], 4))), &a[0]);
}

void test_end_span()
{
    int a[4];
    BOOST_TEST_EQ((end(boost::span<int>(&a[0], 4))), &a[4]);
}

int main()
{
    test_extent();
    test_default_construct_dynamic();
    test_default_construct_static();
    test_construct_data_size();
    test_construct_first_last();
    test_construct_array_dynamic();
    test_construct_array_static();
    test_construct_std_array_dynamic();
    test_construct_std_array_static();
    test_construct_const_std_array_dynamic();
    test_construct_const_std_array_static();
    test_construct_range();
    test_construct_span_dynamic();
    test_construct_span_dynamic_static();
    test_construct_span_static();
    test_construct_span_static_dynamic();
    test_copy_dynamic();
    test_copy_static();
    test_assign_dynamic();
    test_assign_static();
    test_first();
    test_last();
    test_subspan_dynamic();
    test_subspan_static();
    test_subspan();
    test_first_size();
    test_last_size();
    test_subspan_range();
    test_subspan_range_count();
    test_size();
    test_size_bytes();
    test_empty_dynamic();
    test_empty_static();
    test_index();
    test_front();
    test_back();
    test_data();
    test_begin();
    test_end();
    test_rbegin();
    test_rend();
    test_cbegin();
    test_cend();
    test_crbegin();
    test_crend();
    test_begin_span();
    test_end_span();
    return boost::report_errors();
}
#else
int main()
{
    return 0;
}
#endif
