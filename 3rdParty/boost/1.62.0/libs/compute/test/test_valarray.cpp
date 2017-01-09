//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestValarray
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/container/valarray.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(size)
{
    boost::compute::valarray<float> array;
    BOOST_CHECK_EQUAL(array.size(), size_t(0));

    array.resize(10);
    BOOST_CHECK_EQUAL(array.size(), size_t(10));
}

BOOST_AUTO_TEST_CASE(at)
{
    int data[] = { 1, 2, 3, 4, 5 };
    boost::compute::valarray<int> array(data, 5);
    BOOST_CHECK_EQUAL(array.size(), size_t(5));

    boost::compute::system::finish();
    BOOST_CHECK_EQUAL(int(array[0]), int(1));
    BOOST_CHECK_EQUAL(int(array[1]), int(2));
    BOOST_CHECK_EQUAL(int(array[2]), int(3));
    BOOST_CHECK_EQUAL(int(array[3]), int(4));
    BOOST_CHECK_EQUAL(int(array[4]), int(5));
}

BOOST_AUTO_TEST_CASE(min_and_max)
{
    int data[] = { 5, 2, 3, 7, 1, 9, 6, 5 };
    boost::compute::valarray<int> array(data, 8);
    BOOST_CHECK_EQUAL(array.size(), size_t(8));

    BOOST_CHECK_EQUAL((array.min)(), int(1));
    BOOST_CHECK_EQUAL((array.max)(), int(9));
}

BOOST_AUTO_TEST_CASE(sum)
{
    int data[] = { 1, 2, 3, 4 };
    boost::compute::valarray<int> array(data, 4);
    boost::compute::system::finish();

    BOOST_CHECK_EQUAL(array.size(), size_t(4));
    BOOST_CHECK_EQUAL(array.sum(), int(10));
}

BOOST_AUTO_TEST_CASE(apply)
{
    int data[] = { -1, 2, -3, 4 };
    boost::compute::valarray<int> array(data, 4);

    boost::compute::abs<int> abs;
    boost::compute::valarray<int> result = array.apply(abs);
    boost::compute::system::finish();
    BOOST_CHECK_EQUAL(int(result[0]), int(1));
    BOOST_CHECK_EQUAL(int(result[1]), int(2));
    BOOST_CHECK_EQUAL(int(result[2]), int(3));
    BOOST_CHECK_EQUAL(int(result[3]), int(4));
}

/// \internal_
/// Tests for compound assignment operators that works for floating
/// point types.
#define BOOST_COMPUTE_TEST_VALARRAY_COMPOUND_ASSIGNMENT(op, op_name) \
BOOST_AUTO_TEST_CASE(op_name##_ca_operator_no_fp) \
{ \
    float data[] = { 1, 2, 3, 4 }; \
    boost::compute::valarray<float> array1(data, 4); \
    boost::compute::valarray<float> array2(data, 4); \
    boost::compute::system::finish(); \
    \
    array1 op##= 1; \
    boost::compute::system::finish(); \
    BOOST_CHECK_CLOSE(float(array1[0]), float(1.0f op 1.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(array1[1]), float(2.0f op 1.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(array1[2]), float(3.0f op 1.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(array1[3]), float(4.0f op 1.0f), 1e-4f); \
    \
    array1 = boost::compute::valarray<float>(data, 4); \
    boost::compute::system::finish(); \
    \
    array1 op##= array2; \
    boost::compute::system::finish(); \
    BOOST_CHECK_CLOSE(float(array1[0]), float(1.0f op 1.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(array1[1]), float(2.0f op 2.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(array1[2]), float(3.0f op 3.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(array1[3]), float(4.0f op 4.0f), 1e-4f); \
    \
    array2 op##= array2; \
    boost::compute::system::finish(); \
    BOOST_CHECK_CLOSE(float(array2[0]), float(1.0f op 1.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(array2[1]), float(2.0f op 2.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(array2[2]), float(3.0f op 3.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(array2[3]), float(4.0f op 4.0f), 1e-4f); \
    \
}

BOOST_COMPUTE_TEST_VALARRAY_COMPOUND_ASSIGNMENT(+, plus)
BOOST_COMPUTE_TEST_VALARRAY_COMPOUND_ASSIGNMENT(-, minus)
BOOST_COMPUTE_TEST_VALARRAY_COMPOUND_ASSIGNMENT(*, multiplies)
BOOST_COMPUTE_TEST_VALARRAY_COMPOUND_ASSIGNMENT(/, divides)

#undef BOOST_COMPUTE_TEST_VALARRAY_COMPOUND_ASSIGNMENT

/// \internal_
/// Tests for compound assignment operators that does NOT work for floating
/// point types.
/// Note: modulo operator works only for integer types.
#define BOOST_COMPUTE_TEST_VALARRAY_COMPOUND_ASSIGNMENT_NO_FP(op, op_name) \
BOOST_AUTO_TEST_CASE(op_name##_ca_operator) \
{ \
    int data[] = { 1, 2, 3, 4 }; \
    boost::compute::valarray<int> array1(data, 4); \
    boost::compute::valarray<int> array2(data, 4); \
    boost::compute::system::finish(); \
    \
    array1 op##= 1; \
    boost::compute::system::finish(); \
    BOOST_CHECK_EQUAL(int(array1[0]), int(1 op 1)); \
    BOOST_CHECK_EQUAL(int(array1[1]), int(2 op 1)); \
    BOOST_CHECK_EQUAL(int(array1[2]), int(3 op 1)); \
    BOOST_CHECK_EQUAL(int(array1[3]), int(4 op 1)); \
    \
    array1 = boost::compute::valarray<int>(data, 4); \
    boost::compute::system::finish(); \
    \
    array1 op##= array2; \
    boost::compute::system::finish(); \
    BOOST_CHECK_EQUAL(int(array1[0]), int(1 op 1)); \
    BOOST_CHECK_EQUAL(int(array1[1]), int(2 op 2)); \
    BOOST_CHECK_EQUAL(int(array1[2]), int(3 op 3)); \
    BOOST_CHECK_EQUAL(int(array1[3]), int(4 op 4)); \
    \
    array2 op##= array2; \
    boost::compute::system::finish(); \
    BOOST_CHECK_EQUAL(int(array2[0]), int(1 op 1)); \
    BOOST_CHECK_EQUAL(int(array2[1]), int(2 op 2)); \
    BOOST_CHECK_EQUAL(int(array2[2]), int(3 op 3)); \
    BOOST_CHECK_EQUAL(int(array2[3]), int(4 op 4)); \
    \
}

BOOST_COMPUTE_TEST_VALARRAY_COMPOUND_ASSIGNMENT_NO_FP(%, modulus)
BOOST_COMPUTE_TEST_VALARRAY_COMPOUND_ASSIGNMENT_NO_FP(^, bit_xor)
BOOST_COMPUTE_TEST_VALARRAY_COMPOUND_ASSIGNMENT_NO_FP(&, bit_and)
BOOST_COMPUTE_TEST_VALARRAY_COMPOUND_ASSIGNMENT_NO_FP(|, bit_or)
BOOST_COMPUTE_TEST_VALARRAY_COMPOUND_ASSIGNMENT_NO_FP(<<, shift_left)
BOOST_COMPUTE_TEST_VALARRAY_COMPOUND_ASSIGNMENT_NO_FP(>>, shift_right)

#undef BOOST_COMPUTE_TEST_VALARRAY_COMPOUND_ASSIGNMENT_NO_FP

BOOST_AUTO_TEST_CASE(unary_plus_operator)
{
    int data[] = { 1, 2, 3, 4 };
    boost::compute::valarray<int> array(data, 4);
    boost::compute::system::finish();

    boost::compute::valarray<int> result = +array;
    boost::compute::system::finish();
    BOOST_CHECK_EQUAL(int(result[0]), +(int(1)));
    BOOST_CHECK_EQUAL(int(result[1]), +(int(2)));
    BOOST_CHECK_EQUAL(int(result[2]), +(int(3)));
    BOOST_CHECK_EQUAL(int(result[3]), +(int(4)));
}

BOOST_AUTO_TEST_CASE(unary_minus_operator)
{
    int data[] = { -1, 2, 0, 4 };
    boost::compute::valarray<int> array(data, 4);
    boost::compute::system::finish();

    boost::compute::valarray<int> result = -array;
    boost::compute::system::finish();
    BOOST_CHECK_EQUAL(int(result[0]), int(1));
    BOOST_CHECK_EQUAL(int(result[1]), int(-2));
    BOOST_CHECK_EQUAL(int(result[2]), int(0));
    BOOST_CHECK_EQUAL(int(result[3]), int(-4));
}

BOOST_AUTO_TEST_CASE(unary_bitwise_not_operator)
{
    int data[] = { 1, 2, 3, 4 };
    boost::compute::valarray<int> array(data, 4);
    boost::compute::system::finish();

    boost::compute::valarray<int> result = ~array;
    boost::compute::system::finish();
    BOOST_CHECK_EQUAL(int(result[0]), ~(int(1)));
    BOOST_CHECK_EQUAL(int(result[1]), ~(int(2)));
    BOOST_CHECK_EQUAL(int(result[2]), ~(int(3)));
    BOOST_CHECK_EQUAL(int(result[3]), ~(int(4)));
}

BOOST_AUTO_TEST_CASE(unary_logical_not_operator)
{
    int data[] = { 1, -2, 0, 4 };
    boost::compute::valarray<int> array(data, 4);
    boost::compute::system::finish();

    boost::compute::valarray<char> result = !array;
    boost::compute::system::finish();
    BOOST_CHECK_EQUAL(bool(result[0]), !(int(1)));
    BOOST_CHECK_EQUAL(bool(result[1]), !(int(-2)));
    BOOST_CHECK_EQUAL(bool(result[2]), !(int(0)));
    BOOST_CHECK_EQUAL(bool(result[3]), !(int(4)));
}

/// \internal_
/// Tests for binary operators that works for floating
/// point types.
#define BOOST_COMPUTE_TEST_VALARRAY_BINARY_OPERATOR(op, op_name) \
BOOST_AUTO_TEST_CASE(op_name##_binary_operator) \
{ \
    float data1[] = { 1, 2, 3, 4 }; \
    float data2[] = { 4, 2, 3, 0 }; \
    boost::compute::valarray<float> array1(data1, 4); \
    boost::compute::valarray<float> array2(data2, 4); \
    boost::compute::system::finish(); \
    \
    boost::compute::valarray<float> result = 2.0f op array1; \
    boost::compute::system::finish(); \
    BOOST_CHECK_CLOSE(float(result[0]), float(2.0f op 1.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(result[1]), float(2.0f op 2.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(result[2]), float(2.0f op 3.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(result[3]), float(2.0f op 4.0f), 1e-4f); \
    \
    result = array1 op 2.0f; \
    boost::compute::system::finish(); \
    BOOST_CHECK_CLOSE(float(result[0]), float(1.0f op 2.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(result[1]), float(2.0f op 2.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(result[2]), float(3.0f op 2.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(result[3]), float(4.0f op 2.0f), 1e-4f); \
    \
    result = array2 op array1; \
    boost::compute::system::finish(); \
    BOOST_CHECK_CLOSE(float(result[0]), float(4.0f op 1.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(result[1]), float(2.0f op 2.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(result[2]), float(3.0f op 3.0f), 1e-4f); \
    BOOST_CHECK_CLOSE(float(result[3]), float(0.0f op 4.0f), 1e-4f); \
}

BOOST_COMPUTE_TEST_VALARRAY_BINARY_OPERATOR(+, plus)
BOOST_COMPUTE_TEST_VALARRAY_BINARY_OPERATOR(-, minus)
BOOST_COMPUTE_TEST_VALARRAY_BINARY_OPERATOR(*, multiplies)
BOOST_COMPUTE_TEST_VALARRAY_BINARY_OPERATOR(/, divides)

#undef BOOST_COMPUTE_TEST_VALARRAY_BINARY_OPERATOR

/// \internal_
/// Tests for compound assignment operators that does NOT work for floating
/// point types.
/// Note: modulo operator works only for integer types.
#define BOOST_COMPUTE_TEST_VALARRAY_BINARY_OPERATOR_NO_FP(op, op_name) \
BOOST_AUTO_TEST_CASE(op_name##_binary_operator) \
{ \
    int data1[] = { 1, 2, 3, 4 }; \
    int data2[] = { 4, 5, 2, 1 }; \
    boost::compute::valarray<int> array1(data1, 4); \
    boost::compute::valarray<int> array2(data2, 4); \
    boost::compute::system::finish(); \
    \
    boost::compute::valarray<int> result = 5 op array1; \
    boost::compute::system::finish(); \
    BOOST_CHECK_EQUAL(int(result[0]), int(5 op 1)); \
    BOOST_CHECK_EQUAL(int(result[1]), int(5 op 2)); \
    BOOST_CHECK_EQUAL(int(result[2]), int(5 op 3)); \
    BOOST_CHECK_EQUAL(int(result[3]), int(5 op 4)); \
    \
    result = array1 op 5; \
    boost::compute::system::finish(); \
    BOOST_CHECK_EQUAL(int(result[0]), int(1 op 5)); \
    BOOST_CHECK_EQUAL(int(result[1]), int(2 op 5)); \
    BOOST_CHECK_EQUAL(int(result[2]), int(3 op 5)); \
    BOOST_CHECK_EQUAL(int(result[3]), int(4 op 5)); \
    \
    result = array1 op array2; \
    boost::compute::system::finish(); \
    BOOST_CHECK_EQUAL(int(result[0]), int(1 op 4)); \
    BOOST_CHECK_EQUAL(int(result[1]), int(2 op 5)); \
    BOOST_CHECK_EQUAL(int(result[2]), int(3 op 2)); \
    BOOST_CHECK_EQUAL(int(result[3]), int(4 op 1)); \
}

BOOST_COMPUTE_TEST_VALARRAY_BINARY_OPERATOR_NO_FP(^, bit_xor)
BOOST_COMPUTE_TEST_VALARRAY_BINARY_OPERATOR_NO_FP(&, bit_and)
BOOST_COMPUTE_TEST_VALARRAY_BINARY_OPERATOR_NO_FP(|, bit_or)
BOOST_COMPUTE_TEST_VALARRAY_BINARY_OPERATOR_NO_FP(<<, shift_left)
BOOST_COMPUTE_TEST_VALARRAY_BINARY_OPERATOR_NO_FP(>>, shift_right)

#undef BOOST_COMPUTE_TEST_VALARRAY_BINARY_OPERATOR_NO_FP

/// \internal_
/// Macro for generating tests for valarray comparison operators.
#define BOOST_COMPUTE_TEST_VALARRAY_COMPARISON_OPERATOR(op, op_name) \
BOOST_AUTO_TEST_CASE(op_name##_comparision_operator) \
{ \
    int data1[] = { 1, 2, 0, 4 }; \
    int data2[] = { 4, 0, 2, 1 }; \
    boost::compute::valarray<int> array1(data1, 4); \
    boost::compute::valarray<int> array2(data2, 4); \
    boost::compute::system::finish(); \
    \
    boost::compute::valarray<char> result = 2 op array1; \
    boost::compute::system::finish(); \
    BOOST_CHECK_EQUAL(bool(result[0]), bool(2 op 1)); \
    BOOST_CHECK_EQUAL(bool(result[1]), bool(2 op 2)); \
    BOOST_CHECK_EQUAL(bool(result[2]), bool(2 op 0)); \
    BOOST_CHECK_EQUAL(bool(result[3]), bool(2 op 4)); \
    \
    result = array1 op 2; \
    boost::compute::system::finish(); \
    BOOST_CHECK_EQUAL(bool(result[0]), bool(1 op 2)); \
    BOOST_CHECK_EQUAL(bool(result[1]), bool(2 op 2)); \
    BOOST_CHECK_EQUAL(bool(result[2]), bool(0 op 2)); \
    BOOST_CHECK_EQUAL(bool(result[3]), bool(4 op 2)); \
    \
    result = array1 op array2; \
    boost::compute::system::finish(); \
    BOOST_CHECK_EQUAL(bool(result[0]), bool(1 op 4)); \
    BOOST_CHECK_EQUAL(bool(result[1]), bool(2 op 0)); \
    BOOST_CHECK_EQUAL(bool(result[2]), bool(0 op 2)); \
    BOOST_CHECK_EQUAL(bool(result[3]), bool(4 op 1)); \
}

BOOST_COMPUTE_TEST_VALARRAY_COMPARISON_OPERATOR(==, equal_to)
BOOST_COMPUTE_TEST_VALARRAY_COMPARISON_OPERATOR(!=, not_equal_to)
BOOST_COMPUTE_TEST_VALARRAY_COMPARISON_OPERATOR(>, greater)
BOOST_COMPUTE_TEST_VALARRAY_COMPARISON_OPERATOR(<, less)
BOOST_COMPUTE_TEST_VALARRAY_COMPARISON_OPERATOR(>=, greater_equal)
BOOST_COMPUTE_TEST_VALARRAY_COMPARISON_OPERATOR(<=, less_equal)

/// \internal_
/// Macro for generating tests for valarray binary logical operators.
#define BOOST_COMPUTE_TEST_VALARRAY_LOGICAL_OPERATOR(op, op_name) \
    BOOST_COMPUTE_TEST_VALARRAY_COMPARISON_OPERATOR(op, op_name)

BOOST_COMPUTE_TEST_VALARRAY_LOGICAL_OPERATOR(&&, logical_and)
BOOST_COMPUTE_TEST_VALARRAY_LOGICAL_OPERATOR(||, logical_or)

#undef BOOST_COMPUTE_TEST_VALARRAY_LOGICAL_OPERATOR

#undef BOOST_COMPUTE_TEST_VALARRAY_COMPARISON_OPERATOR

BOOST_AUTO_TEST_SUITE_END()
