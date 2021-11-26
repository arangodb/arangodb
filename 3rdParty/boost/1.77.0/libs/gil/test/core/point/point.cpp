//
// Copyright 2018-2020 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/point.hpp>

#include <boost/core/lightweight_test.hpp>

#include <type_traits>

#include "test_utility_output_stream.hpp"

namespace gil = boost::gil;

void test_default_constructor()
{
    gil::point<int> p;
    BOOST_TEST_EQ(p.x, 0);
    BOOST_TEST_EQ(p.y, 0);
}

void test_user_constructor()
{
    gil::point<int> p{1, 2};
    BOOST_TEST_EQ(p.x, 1);
    BOOST_TEST_EQ(p.y, 2);
}

void test_copy_constructor()
{
    gil::point<int> p1{1, 2};
    gil::point<int> p2{p1};
    BOOST_TEST_EQ(p1.x, p2.x);
    BOOST_TEST_EQ(p1.y, p2.y);
}

void test_copy_assignment_operator()
{
    gil::point<int> p1{1, 2};
    gil::point<int> p2;
    p2 = p1;
    BOOST_TEST_EQ(p1.x, p2.x);
    BOOST_TEST_EQ(p1.y, p2.y);
}

void test_index_operator()
{
    gil::point<int> p{1, 2};
    BOOST_TEST_EQ(p[0], 1);
    BOOST_TEST_EQ(p[1], 2);
}

void test_addition_operator()
{
    gil::point<int> p1{1, 1};
    gil::point<int> const p2{2, 4};
    auto p3 = p1 + p2;
    BOOST_TEST_EQ(p3.x, 3);
    BOOST_TEST_EQ(p3.y, 5);
}

void test_addition_assignment_operator()
{
    gil::point<int> p1{1, 1};
    gil::point<int> const p2{2, 4};
    p1 += p2;
    BOOST_TEST_EQ(p1.x, 3);
    BOOST_TEST_EQ(p1.y, 5);
}

void test_subtraction_assignment_operator()
{
    gil::point<int> p1{2, 4};
    gil::point<int> const p2{1, 1};
    p1 -= p2;
    BOOST_TEST_EQ(p1.x, 1);
    BOOST_TEST_EQ(p1.y, 3);
}

void test_subtraction_operator()
{
    gil::point<int> p1{2, 4};
    gil::point<int> const p2{1, 1};
    p1 = p1 - p2;
    BOOST_TEST_EQ(p1.x, 1);
    BOOST_TEST_EQ(p1.y, 3);
}

void test_unary_minus_operator()
{
    gil::point<int> p1{2, 4};
    auto p2 = -p1;
    BOOST_TEST_EQ(p2.x, -2);
    BOOST_TEST_EQ(p2.y, -4);
    p2 = -p2;
    BOOST_TEST_EQ(p2.x, p1.x);
    BOOST_TEST_EQ(p2.y, p1.y);
}

void test_division_assignment_operator()
{
    {
        gil::point<int> p1{2, 4};
        p1 /= 2;
        static_assert(std::is_same<decltype((p1 / short{}).x), int>::value, "!int");
        static_assert(std::is_same<decltype((p1 / int{}).x), int>::value, "!int");
        static_assert(std::is_same<decltype((p1 / float{}).x), float>::value, "!float");
        static_assert(std::is_same<decltype((p1 / double{}).x), double>::value, "!double");
        BOOST_TEST_EQ(p1.x, 1);
        BOOST_TEST_EQ(p1.y, 2);
    }
    // point / d
    {
        gil::point<int> p1{2, 4};
        auto p2 = p1 / float{2};
        static_assert(std::is_same<decltype((p2 / int{}).x), float>::value, "!float");
        static_assert(std::is_same<decltype((p2 / float{}).x), float>::value, "!float");
        static_assert(std::is_same<decltype((p2 / double{}).x), double>::value, "!double");
        BOOST_TEST_GE(p2.x, 1.0); // means, but >= avoids compiler warning
        BOOST_TEST_GE(p2.y, 2.0);
    }
}

void test_multiplication_operator()
{
    gil::point<int> p1{2, 4};
    p1 *= 2;
    BOOST_TEST_EQ(p1.x, 4);
    BOOST_TEST_EQ(p1.y, 8);

    // point * m
    {
        auto p2 = p1 * int{2};
        static_assert(std::is_same<decltype(p2.x), int>::value, "!int");
        static_assert(std::is_same<decltype(p2.y), int>::value, "!int");
        BOOST_TEST_EQ(p2.x, 8);
        BOOST_TEST_EQ(p2.y, 16);
    }
    // m * point
    {
        auto p2 = double{2} *p1;
        static_assert(std::is_same<decltype(p2.x), double>::value, "!double");
        static_assert(std::is_same<decltype(p2.y), double>::value, "!double");
        BOOST_TEST_GE(p2.x, 8); // means, but >= avoids compiler warning
        BOOST_TEST_GE(p2.y, 16);
    }
}

void test_multiplication_assignment_operator()
{
    gil::point<int> p1{2, 4};
    // point * m
    {
        auto p2 = p1 * int{2};
        static_assert(std::is_same<decltype(p2.x), int>::value, "!int");
        static_assert(std::is_same<decltype(p2.y), int>::value, "!int");
        BOOST_TEST_EQ(p2.x, 4);
        BOOST_TEST_EQ(p2.y, 8);
    }
    // m * point
    {
        auto p2 = double{2} * p1;
        static_assert(std::is_same<decltype(p2.x), double>::value, "!double");
        static_assert(std::is_same<decltype(p2.y), double>::value, "!double");
        BOOST_TEST_GE(p2.x, 4); // means, but >= avoids compiler warning
        BOOST_TEST_GE(p2.y, 8);
    }
}

void test_bitwise_left_shift_operator()
{
    gil::point<unsigned int> p{2, 4};
    p = p << 1;
    BOOST_TEST_EQ(p.x, 4u);
    BOOST_TEST_EQ(p.y, 8u);
}

void test_bitwise_right_shift_operator()
{
    gil::point<unsigned int> p{2, 4};
    p = p >> 1;
    BOOST_TEST_EQ(p.x, 2u / 2);
    BOOST_TEST_EQ(p.y, 4u / 2);
}

void test_equal_to_operator()
{
    gil::point<int> p1{2, 4};
    gil::point<int> p2{2, 4};
    BOOST_TEST_EQ(p1, p2);
}

void test_not_equal_to_operator()
{
    gil::point<int> p1{1, 1};
    gil::point<int> p2{2, 4};
    BOOST_TEST_NE(p1, p2);
}

void test_axis_value()
{
    gil::point<int> p1{1, 2};
    gil::point<int> const p2{1, 2};
    BOOST_TEST_EQ(gil::axis_value<0>(p1), p1.x);
    BOOST_TEST_EQ(gil::axis_value<1>(p1), p1.y);
    BOOST_TEST_EQ(gil::axis_value<0>(p2), p2.x);
    BOOST_TEST_EQ(gil::axis_value<1>(p2), p2.y);
}

int main()
{
    test_default_constructor();
    test_user_constructor();
    test_copy_constructor();
    test_copy_assignment_operator();
    test_index_operator();
    test_addition_operator();
    test_addition_assignment_operator();
    test_subtraction_assignment_operator();
    test_subtraction_operator();
    test_unary_minus_operator();
    test_division_assignment_operator();
    test_multiplication_operator();
    test_multiplication_assignment_operator();
    test_bitwise_left_shift_operator();
    test_bitwise_right_shift_operator();
    test_equal_to_operator();
    test_not_equal_to_operator();
    test_axis_value();

    return boost::report_errors();
}
