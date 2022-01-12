//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
// Copyright 2019 Miral Shah <miralshah2211@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_DISABLE_ASSERTS 1 // kernel_1d_adaptor assertions are too strict
#include <boost/gil.hpp>
#include <boost/gil/extension/numeric/kernel.hpp>

#include <boost/core/lightweight_test.hpp>

#include <vector>

namespace gil = boost::gil;

void test_kernel_1d_default_constructor()
{
    gil::kernel_1d<int> k;
    BOOST_TEST_EQ(k.center(), 0);
    BOOST_TEST_EQ(k.left_size(), 0);
    BOOST_TEST_EQ(static_cast<int>(k.right_size()), -1);
    // std::vector interface
    BOOST_TEST_EQ(k.size(), 0);
}

void test_kernel_2d_default_constructor()
{
    gil::detail::kernel_2d<int> k;
    BOOST_TEST_EQ(k.center_y(), 0);
    BOOST_TEST_EQ(k.center_x(), 0);

    //BOOST_TEST_EQ(k.left_size(), 0);
    //BOOST_TEST_EQ(k.right_size(), -1);
    BOOST_TEST_EQ(k.upper_size(), 0);
    BOOST_TEST_EQ(static_cast<int>(k.lower_size()), -1);
    // std::vector interface
    BOOST_TEST_EQ(k.size(), 0);
}

void test_kernel_1d_parameterized_constructor()
{
    gil::kernel_1d<int> k(9, 4);
    BOOST_TEST_EQ(k.center(), 4);
    BOOST_TEST_EQ(k.left_size(), 4);
    BOOST_TEST_EQ(k.right_size(), 4);
    // std::vector interface
    BOOST_TEST_EQ(k.size(), 9);
}

void test_kernel_2d_parameterized_constructor()
{
    gil::detail::kernel_2d<int> k(9, 4, 4);
    BOOST_TEST_EQ(k.center_y(), 4);
    BOOST_TEST_EQ(k.center_x(), 4);
    BOOST_TEST_EQ(k.left_size(), 4);
    BOOST_TEST_EQ(k.right_size(), 4);
    BOOST_TEST_EQ(k.upper_size(), 4);
    BOOST_TEST_EQ(k.lower_size(), 4);
    // std::vector interface
    BOOST_TEST_EQ(k.size(), 9);
}

void test_kernel_1d_parameterized_constructor_with_iterator()
{
    std::vector<int> v(9);
    gil::kernel_1d<int> k(v.cbegin(), v.size(), 4);
    BOOST_TEST_EQ(k.center(), 4);
    BOOST_TEST_EQ(k.left_size(), 4);
    BOOST_TEST_EQ(k.right_size(), 4);
    // std::vector interface
    BOOST_TEST_EQ(k.size(), 9);
}

void test_kernel_2d_parameterized_constructor_with_iterator()
{
    std::vector<int> v(81);
    gil::detail::kernel_2d<int> k(v.cbegin(), v.size(), 4, 4);
    BOOST_TEST_EQ(k.center_y(), 4);
    BOOST_TEST_EQ(k.center_x(), 4);
    BOOST_TEST_EQ(k.left_size(), 4);
    BOOST_TEST_EQ(k.right_size(), 4);
    BOOST_TEST_EQ(k.upper_size(), 4);
    BOOST_TEST_EQ(k.lower_size(), 4);
    // std::vector interface
    BOOST_TEST_EQ(k.size(), 9);
}

void test_kernel_1d_copy_constructor()
{
    gil::kernel_1d<int> d(9, 4);
    gil::kernel_1d<int> k(d);
    BOOST_TEST_EQ(k.center(), 4);
    BOOST_TEST_EQ(k.center(), d.center());
    BOOST_TEST_EQ(k.left_size(), d.left_size());
    BOOST_TEST_EQ(k.right_size(), d.right_size());
    // std::vector interface
    BOOST_TEST_EQ(k.size(), d.size());
}

void test_kernel_2d_copy_constructor()
{
    gil::detail::kernel_2d<int> d(9, 4, 4);
    gil::detail::kernel_2d<int> k(d);
    BOOST_TEST_EQ(k.center_y(), 4);
    BOOST_TEST_EQ(k.center_x(), 4);
    BOOST_TEST_EQ(k.center_y(), d.center_y());
    BOOST_TEST_EQ(k.center_x(), d.center_x());
    BOOST_TEST_EQ(k.left_size(), d.left_size());
    BOOST_TEST_EQ(k.right_size(), d.right_size());
    BOOST_TEST_EQ(k.lower_size(), d.lower_size());
    BOOST_TEST_EQ(k.upper_size(), d.upper_size());
    // std::vector interface
    BOOST_TEST_EQ(k.size(), d.size());
}

void test_kernel_1d_assignment_operator()
{
    gil::kernel_1d<int> d(9, 4);
    gil::kernel_1d<int> k;
    k = d;
    BOOST_TEST_EQ(k.center(), 4);
    BOOST_TEST_EQ(k.center(), d.center());
    BOOST_TEST_EQ(k.left_size(), d.left_size());
    BOOST_TEST_EQ(k.right_size(), d.right_size());
    // std::vector interface
    BOOST_TEST_EQ(k.size(), d.size());
}

void test_kernel_2d_assignment_operator()
{
    gil::detail::kernel_2d<int> d(9, 4, 4);
    gil::detail::kernel_2d<int> k;
    k = d;
    BOOST_TEST_EQ(k.center_y(), 4);
    BOOST_TEST_EQ(k.center_x(), 4);
    BOOST_TEST_EQ(k.center_y(), d.center_y());
    BOOST_TEST_EQ(k.center_x(), d.center_x());
    BOOST_TEST_EQ(k.left_size(), d.left_size());
    BOOST_TEST_EQ(k.right_size(), d.right_size());
    BOOST_TEST_EQ(k.lower_size(), d.lower_size());
    BOOST_TEST_EQ(k.upper_size(), d.upper_size());
    // std::vector interface
    BOOST_TEST_EQ(k.size(), d.size());
}

void test_kernel_1d_reverse_kernel()
{
    gil::kernel_1d<int> d(12, 4);
    BOOST_TEST_EQ(d.center(), 4);
    auto k = gil::reverse_kernel(d);
    BOOST_TEST_EQ(k.center(), d.right_size());
    // std::vector interface
    BOOST_TEST_EQ(k.size(), d.size());
}

void test_kernel_2d_reverse_kernel()
{
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    gil::detail::kernel_2d<int> d(data.begin(), data.size(), 2, 0);

    BOOST_TEST_EQ(d.center_x(), 0);
    BOOST_TEST_EQ(d.center_y(), 2);

    auto k = gil::detail::reverse_kernel(d);

    BOOST_TEST_EQ(k.center_x(), d.lower_size());
    BOOST_TEST_EQ(k.center_x(), 0);

    BOOST_TEST_EQ(k.center_y(), d.right_size());
    BOOST_TEST_EQ(k.center_y(), 2);

    // std::vector interface
    BOOST_TEST_EQ(k.size(), d.size());

    for (size_t i = 0; i < k.size() * k.size(); i++)
    {
        BOOST_TEST_EQ(d[i], k[(k.size()*k.size()) - 1 - i]);
    }
}

int main()
{
    test_kernel_1d_default_constructor();
    test_kernel_2d_default_constructor();
    test_kernel_1d_parameterized_constructor();
    test_kernel_2d_parameterized_constructor();
    test_kernel_1d_parameterized_constructor_with_iterator();
    test_kernel_2d_parameterized_constructor_with_iterator();
    test_kernel_1d_copy_constructor();
    test_kernel_2d_copy_constructor();
    test_kernel_1d_assignment_operator();
    test_kernel_2d_assignment_operator();
    test_kernel_1d_reverse_kernel();
    test_kernel_2d_reverse_kernel();

    return ::boost::report_errors();
}
