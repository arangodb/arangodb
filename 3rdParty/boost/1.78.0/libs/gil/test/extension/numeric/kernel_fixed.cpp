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

void test_kernel_1d_fixed_default_constructor()
{
    gil::kernel_1d_fixed<int, 9> k;
    BOOST_TEST_EQ(k.center(), 0);
    BOOST_TEST_EQ(k.left_size(), 0);
    BOOST_TEST_EQ(k.right_size(), 8); // TODO: Why not 0 or -1 if not set?
    // std::array interface
    BOOST_TEST_EQ(k.size(), 9);
}

void test_kernel_2d_fixed_default_constructor()
{
    gil::detail::kernel_2d_fixed<int, 9> k;
    BOOST_TEST_EQ(k.center_x(), 0);
    BOOST_TEST_EQ(k.center_y(), 0);
    BOOST_TEST_EQ(k.left_size(), 0);
    BOOST_TEST_EQ(k.right_size(), 8); // TODO: Why not 0 or -1 if not set?
    BOOST_TEST_EQ(k.upper_size(), 0);
    BOOST_TEST_EQ(k.lower_size(), 8);
    // std::array interface
    BOOST_TEST_EQ(k.size(), 9);
}

void test_kernel_1d_fixed_parameterized_constructor()
{
    gil::kernel_1d_fixed<int, 9> k(4);
    BOOST_TEST_EQ(k.center(), 4);
    BOOST_TEST_EQ(k.left_size(), 4);
    BOOST_TEST_EQ(k.right_size(), 4);
    // std::vector interface
    BOOST_TEST_EQ(k.size(), 9);
}

void test_kernel_2d_fixed_parameterized_constructor()
{
    gil::detail::kernel_2d_fixed<int, 9> k(4, 4);
    BOOST_TEST_EQ(k.center_x(), 4);
    BOOST_TEST_EQ(k.center_y(), 4);
    BOOST_TEST_EQ(k.left_size(), 4);
    BOOST_TEST_EQ(k.right_size(), 4);
    BOOST_TEST_EQ(k.upper_size(), 4);
    BOOST_TEST_EQ(k.lower_size(), 4);
    // std::vector interface
    BOOST_TEST_EQ(k.size(), 9);
}

void test_kernel_1d_fixed_parameterized_constructor_with_iterator()
{
    // FIXME: The constructor should throw if v.size() < k.size()
    std::vector<int> v(9);
    gil::kernel_1d_fixed<int, 9> k(v.cbegin(), 4);
    BOOST_TEST_EQ((gil::kernel_1d_fixed<int, 9>::static_size), 9);
    BOOST_TEST_EQ(k.center(), 4);
    BOOST_TEST_EQ(k.left_size(), 4);
    BOOST_TEST_EQ(k.right_size(), 4);
    // std::vector interface
    BOOST_TEST_EQ(k.size(), 9);
}

void test_kernel_2d_fixed_parameterized_constructor_with_iterator()
{
//    // FIXME: The constructor should throw if v.size() < k.size()
    std::array<int, 81> v;
    gil::detail::kernel_2d_fixed<int, 9> k(v.cbegin(), 4, 4);
    BOOST_TEST_EQ((gil::detail::kernel_2d_fixed<int, 9>::static_size), 9);
    BOOST_TEST_EQ(k.center_y(), 4);
    BOOST_TEST_EQ(k.center_x(), 4);
    BOOST_TEST_EQ(k.left_size(), 4);
    BOOST_TEST_EQ(k.right_size(), 4);
    BOOST_TEST_EQ(k.upper_size(), 4);
    BOOST_TEST_EQ(k.lower_size(), 4);
    // std::vector interface
    BOOST_TEST_EQ(k.size(), 9);
}

void test_kernel_1d_fixed_copy_constructor()
{
    gil::kernel_1d_fixed<int, 9> d(4);
    gil::kernel_1d_fixed<int, 9> k(d);
    BOOST_TEST_EQ((gil::kernel_1d_fixed<int, 9>::static_size), 9);
    BOOST_TEST_EQ(k.center(), 4);
    BOOST_TEST_EQ(k.center(), d.center());
    BOOST_TEST_EQ(k.left_size(), d.left_size());
    BOOST_TEST_EQ(k.right_size(), d.right_size());
    // std::vector interface
    BOOST_TEST_EQ(k.size(), d.size());
}

void test_kernel_2d_fixed_copy_constructor()
{
    gil::detail::kernel_2d_fixed<int, 9> d(4, 4);
    gil::detail::kernel_2d_fixed<int, 9> k(d);
    BOOST_TEST_EQ((gil::detail::kernel_2d_fixed<int, 9>::static_size), 9);
    BOOST_TEST_EQ(k.center_x(), 4);
    BOOST_TEST_EQ(k.center_y(), 4);
    BOOST_TEST_EQ(k.center_x(), d.center_x());
    BOOST_TEST_EQ(k.center_y(), d.center_y());
    BOOST_TEST_EQ(k.left_size(), d.left_size());
    BOOST_TEST_EQ(k.right_size(), d.right_size());
    BOOST_TEST_EQ(k.lower_size(), d.lower_size());
    BOOST_TEST_EQ(k.upper_size(), d.upper_size());
    // std::vector interface
    BOOST_TEST_EQ(k.size(), d.size());
}

void test_kernel_1d_fixed_assignment_operator()
{
    gil::kernel_1d_fixed<int, 9> d(4);
    gil::kernel_1d_fixed<int, 9> k;
    k = d;
    BOOST_TEST_EQ((gil::kernel_1d_fixed<int, 9>::static_size), 9);
    BOOST_TEST_EQ(k.center(), 4);
    BOOST_TEST_EQ(k.center(), d.center());
    BOOST_TEST_EQ(k.left_size(), d.left_size());
    BOOST_TEST_EQ(k.right_size(), d.right_size());
    // std::vector interface
    BOOST_TEST_EQ(k.size(), d.size());
}

void test_kernel_2d_fixed_assignment_operator()
{
    gil::detail::kernel_2d_fixed<int, 9> d(4, 4);
    gil::detail::kernel_2d_fixed<int, 9> k;
    k = d;
    BOOST_TEST_EQ((gil::detail::kernel_2d_fixed<int, 9>::static_size), 9);
    BOOST_TEST_EQ(k.center_x(), 4);
    BOOST_TEST_EQ(k.center_y(), 4);
    BOOST_TEST_EQ(k.center_x(), d.center_x());
    BOOST_TEST_EQ(k.center_y(), d.center_y());
    BOOST_TEST_EQ(k.left_size(), d.left_size());
    BOOST_TEST_EQ(k.right_size(), d.right_size());
    BOOST_TEST_EQ(k.lower_size(), d.lower_size());
    BOOST_TEST_EQ(k.upper_size(), d.upper_size());
    // std::vector interface
    BOOST_TEST_EQ(k.size(), d.size());
}

void test_kernel_1d_fixed_reverse_kernel()
{
    std::array<int, 3> values = {{1, 2, 3}};
    gil::kernel_1d_fixed<int, 3> k(values.begin(), 1);
    BOOST_TEST_EQ((gil::kernel_1d_fixed<int, 3>::static_size), 3);
    BOOST_TEST_ALL_EQ(k.begin(), k.end(), values.begin(), values.end());

    std::array<int, 3> values_rev = {{3, 2, 1}};
    auto const k_rev              = gil::reverse_kernel(k);
    BOOST_TEST_ALL_EQ(k_rev.begin(), k_rev.end(), values_rev.begin(), values_rev.end());
}

int main()
{
    test_kernel_1d_fixed_default_constructor();
    test_kernel_2d_fixed_default_constructor();
    test_kernel_1d_fixed_parameterized_constructor();
    test_kernel_2d_fixed_parameterized_constructor();
    test_kernel_1d_fixed_parameterized_constructor_with_iterator();
    test_kernel_2d_fixed_parameterized_constructor_with_iterator();
    test_kernel_1d_fixed_copy_constructor();
    test_kernel_2d_fixed_copy_constructor();
    test_kernel_1d_fixed_assignment_operator();
    test_kernel_2d_fixed_assignment_operator();
    test_kernel_1d_fixed_reverse_kernel();

    return ::boost::report_errors();
}
