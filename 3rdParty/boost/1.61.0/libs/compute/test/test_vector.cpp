//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestVector
#include <boost/test/unit_test.hpp>

#include <boost/concept_check.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/fill.hpp>
#include <boost/compute/algorithm/find.hpp>
#include <boost/compute/algorithm/remove.hpp>
#include <boost/compute/allocator/pinned_allocator.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;
namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(concept_check)
{
    BOOST_CONCEPT_ASSERT((boost::Container<bc::vector<int> >));
    //BOOST_CONCEPT_ASSERT((boost::SequenceConcept<bc::vector<int> >));
    BOOST_CONCEPT_ASSERT((boost::ReversibleContainer<bc::vector<int> >));
    BOOST_CONCEPT_ASSERT((boost::RandomAccessIterator<bc::vector<int>::iterator>));
    BOOST_CONCEPT_ASSERT((boost::RandomAccessIterator<bc::vector<int>::const_iterator>));
}

BOOST_AUTO_TEST_CASE(size)
{
    bc::vector<int> empty_vector(context);
    BOOST_CHECK_EQUAL(empty_vector.size(), size_t(0));
    BOOST_CHECK_EQUAL(empty_vector.empty(), true);

    bc::vector<int> int_vector(10, context);
    BOOST_CHECK_EQUAL(int_vector.size(), size_t(10));
    BOOST_CHECK_EQUAL(int_vector.empty(), false);
}

BOOST_AUTO_TEST_CASE(resize)
{
    bc::vector<int> int_vector(10, context);
    BOOST_CHECK_EQUAL(int_vector.size(), size_t(10));

    int_vector.resize(20, queue);
    BOOST_CHECK_EQUAL(int_vector.size(), size_t(20));

    int_vector.resize(5, queue);
    BOOST_CHECK_EQUAL(int_vector.size(), size_t(5));
}

BOOST_AUTO_TEST_CASE(array_operator)
{
    bc::vector<int> vector(10);
    bc::fill(vector.begin(), vector.end(), 0);
    CHECK_RANGE_EQUAL(int, 10, vector, (0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

    bc::fill(vector.begin(), vector.end(), 42);
    CHECK_RANGE_EQUAL(int, 10, vector, (42, 42, 42, 42, 42, 42, 42, 42, 42, 42));

    vector[0] = 9;
    CHECK_RANGE_EQUAL(int, 10, vector, (9, 42, 42, 42, 42, 42, 42, 42, 42, 42));
}

BOOST_AUTO_TEST_CASE(front_and_back)
{
    int int_data[] = { 1, 2, 3, 4, 5 };
    bc::vector<int> int_vector(5, context);
    bc::copy(int_data, int_data + 5, int_vector.begin(), queue);
    queue.finish();
    BOOST_CHECK_EQUAL(int_vector.front(), 1);
    BOOST_CHECK_EQUAL(int_vector.back(), 5);

    bc::fill(int_vector.begin(), int_vector.end(), 10, queue);
    queue.finish();
    BOOST_CHECK_EQUAL(int_vector.front(), 10);
    BOOST_CHECK_EQUAL(int_vector.back(), 10);

    float float_data[] = { 1.1f, 2.2f, 3.3f, 4.4f, 5.5f };
    bc::vector<float> float_vector(5, context);
    bc::copy(float_data, float_data + 5, float_vector.begin(), queue);
    queue.finish();
    BOOST_CHECK_EQUAL(float_vector.front(), 1.1f);
    BOOST_CHECK_EQUAL(float_vector.back(), 5.5f);
}

BOOST_AUTO_TEST_CASE(host_iterator_constructor)
{
    std::vector<int> host_vector;
    host_vector.push_back(10);
    host_vector.push_back(20);
    host_vector.push_back(30);
    host_vector.push_back(40);

    bc::vector<int> device_vector(host_vector.begin(), host_vector.end(),
                                  queue);
    CHECK_RANGE_EQUAL(int, 4, device_vector, (10, 20, 30, 40));
}

BOOST_AUTO_TEST_CASE(device_iterator_constructor)
{
    int data[] = { 1, 5, 10, 15 };
    bc::vector<int> a(data, data + 4, queue);
    CHECK_RANGE_EQUAL(int, 4, a, (1, 5, 10, 15));

    bc::vector<int> b(a.begin(), a.end(), queue);
    CHECK_RANGE_EQUAL(int, 4, b, (1, 5, 10, 15));
}

BOOST_AUTO_TEST_CASE(push_back)
{
    bc::vector<int> vector(context);
    BOOST_VERIFY(vector.empty());

    vector.push_back(12, queue);
    BOOST_VERIFY(!vector.empty());
    BOOST_CHECK_EQUAL(vector.size(), size_t(1));
    CHECK_RANGE_EQUAL(int, 1, vector, (12));

    vector.push_back(24, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(2));
    CHECK_RANGE_EQUAL(int, 2, vector, (12, 24));

    vector.push_back(36, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(3));
    CHECK_RANGE_EQUAL(int, 3, vector, (12, 24, 36));

    for(int i = 0; i < 100; i++){
        vector.push_back(i, queue);
    }
    BOOST_CHECK_EQUAL(vector.size(), size_t(103));
    BOOST_CHECK_EQUAL(vector[0], 12);
    BOOST_CHECK_EQUAL(vector[1], 24);
    BOOST_CHECK_EQUAL(vector[2], 36);
    BOOST_CHECK_EQUAL(vector[102], 99);
}

BOOST_AUTO_TEST_CASE(at)
{
    bc::vector<int> vector(context);
    vector.push_back(1, queue);
    vector.push_back(2, queue);
    vector.push_back(3, queue);
    BOOST_CHECK_EQUAL(vector.at(0), 1);
    BOOST_CHECK_EQUAL(vector.at(1), 2);
    BOOST_CHECK_EQUAL(vector.at(2), 3);
    BOOST_CHECK_THROW(vector.at(3), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(erase)
{
    int data[] = { 1, 2, 5, 7, 9 };
    bc::vector<int> vector(data, data + 5, queue);
    queue.finish();
    BOOST_CHECK_EQUAL(vector.size(), size_t(5));

    vector.erase(vector.begin() + 1, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(4));
    CHECK_RANGE_EQUAL(int, 4, vector, (1, 5, 7, 9));

    vector.erase(vector.begin() + 2, vector.end(), queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(2));
    CHECK_RANGE_EQUAL(int, 2, vector, (1, 5));
}

BOOST_AUTO_TEST_CASE(max_size)
{
    bc::vector<int> vector(100, context);
    BOOST_CHECK_EQUAL(vector.size(), size_t(100));
    BOOST_VERIFY(vector.max_size() > vector.size());
}

#ifndef BOOST_COMPUTE_NO_RVALUE_REFERENCES
BOOST_AUTO_TEST_CASE(move_ctor)
{
      int data[] = { 11, 12, 13, 14 };
      bc::vector<int> a(data, data + 4, queue);
      BOOST_CHECK_EQUAL(a.size(), size_t(4));
      CHECK_RANGE_EQUAL(int, 4, a, (11, 12, 13, 14));

      bc::vector<int> b(std::move(a));
      BOOST_CHECK(a.size() == 0);
      BOOST_CHECK(a.get_buffer().get() == 0);
      BOOST_CHECK_EQUAL(b.size(), size_t(4));
      CHECK_RANGE_EQUAL(int, 4, b, (11, 12, 13, 14));
}

BOOST_AUTO_TEST_CASE(move_ctor_custom_alloc)
{
      int data[] = { 11, 12, 13, 14 };
      bc::vector<int, bc::pinned_allocator<int> > a(data, data + 4, queue);
      BOOST_CHECK_EQUAL(a.size(), size_t(4));
      CHECK_RANGE_EQUAL(int, 4, a, (11, 12, 13, 14));

      bc::vector<int, bc::pinned_allocator<int> > b(std::move(a));
      BOOST_CHECK(a.size() == 0);
      BOOST_CHECK(a.get_buffer().get() == 0);
      BOOST_CHECK_EQUAL(b.size(), size_t(4));
      CHECK_RANGE_EQUAL(int, 4, b, (11, 12, 13, 14));
}
#endif // BOOST_COMPUTE_NO_RVALUE_REFERENCES

#ifdef BOOST_COMPUTE_USE_CPP11
BOOST_AUTO_TEST_CASE(initializer_list_ctor)
{
    bc::vector<int> vector = { 2, 4, 6, 8 };
    BOOST_CHECK_EQUAL(vector.size(), size_t(4));
    CHECK_RANGE_EQUAL(int, 4, vector, (2, 4, 6, 8));
}
#endif // BOOST_COMPUTE_USE_CPP11

BOOST_AUTO_TEST_CASE(vector_double)
{
    if(!device.supports_extension("cl_khr_fp64")){
        return;
    }

    bc::vector<double> vector(context);
    vector.push_back(1.21, queue);
    vector.push_back(3.14, queue);
    vector.push_back(7.89, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(3));
    CHECK_RANGE_EQUAL(double, 3, vector, (1.21, 3.14, 7.89));

    bc::vector<double> other(vector.begin(), vector.end(), queue);
    CHECK_RANGE_EQUAL(double, 3, other, (1.21, 3.14, 7.89));

    bc::fill(other.begin(), other.end(), 8.95, queue);
    CHECK_RANGE_EQUAL(double, 3, other, (8.95, 8.95, 8.95));
}

BOOST_AUTO_TEST_CASE(vector_iterator)
{
    bc::vector<int> vector(context);
    vector.push_back(2, queue);
    vector.push_back(4, queue);
    vector.push_back(6, queue);
    vector.push_back(8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(4));
    BOOST_CHECK_EQUAL(vector[0], 2);
    BOOST_CHECK_EQUAL(*vector.begin(), 2);
    BOOST_CHECK_EQUAL(vector.begin()[0], 2);
    BOOST_CHECK_EQUAL(vector[1], 4);
    BOOST_CHECK_EQUAL(*(vector.begin()+1), 4);
    BOOST_CHECK_EQUAL(vector.begin()[1], 4);
    BOOST_CHECK_EQUAL(vector[2], 6);
    BOOST_CHECK_EQUAL(*(vector.begin()+2), 6);
    BOOST_CHECK_EQUAL(vector.begin()[2], 6);
    BOOST_CHECK_EQUAL(vector[3], 8);
    BOOST_CHECK_EQUAL(*(vector.begin()+3), 8);
    BOOST_CHECK_EQUAL(vector.begin()[3], 8);
}

BOOST_AUTO_TEST_CASE(vector_erase_remove)
{
    int data[] = { 2, 6, 3, 4, 2, 4, 5, 6, 1 };
    bc::vector<int> vector(data, data + 9, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(9));

    // remove 4's
    vector.erase(bc::remove(vector.begin(), vector.end(), 4, queue), vector.end());
    BOOST_CHECK_EQUAL(vector.size(), size_t(7));
    BOOST_VERIFY(bc::find(vector.begin(), vector.end(), 4, queue) == vector.end());

    // remove 2's
    vector.erase(bc::remove(vector.begin(), vector.end(), 2, queue), vector.end());
    BOOST_CHECK_EQUAL(vector.size(), size_t(5));
    BOOST_VERIFY(bc::find(vector.begin(), vector.end(), 2, queue) == vector.end());

    // remove 6's
    vector.erase(bc::remove(vector.begin(), vector.end(), 6, queue), vector.end());
    BOOST_CHECK_EQUAL(vector.size(), size_t(3));
    BOOST_VERIFY(bc::find(vector.begin(), vector.end(), 6, queue) == vector.end());

    // check the rest of the values
    CHECK_RANGE_EQUAL(int, 3, vector, (3, 5, 1));
}

// see issue #132 (https://github.com/boostorg/compute/issues/132)
BOOST_AUTO_TEST_CASE(swap_between_contexts)
{
    compute::context ctx1(device);
    compute::context ctx2(device);

    compute::vector<int> vec1(32, ctx1);
    compute::vector<int> vec2(32, ctx2);

    BOOST_CHECK(vec1.get_allocator().get_context() == ctx1);
    BOOST_CHECK(vec2.get_allocator().get_context() == ctx2);

    vec1.swap(vec2);

    BOOST_CHECK(vec1.get_allocator().get_context() == ctx2);
    BOOST_CHECK(vec2.get_allocator().get_context() == ctx1);

    vec1.resize(64);
    vec2.resize(64);
}

BOOST_AUTO_TEST_CASE(assign_from_std_vector)
{
    std::vector<int> host_vector;
    host_vector.push_back(1);
    host_vector.push_back(9);
    host_vector.push_back(7);
    host_vector.push_back(9);

    compute::vector<int> device_vector(context);
    device_vector.assign(host_vector.begin(), host_vector.end(), queue);
    BOOST_CHECK_EQUAL(device_vector.size(), size_t(4));
    CHECK_RANGE_EQUAL(int, 4, device_vector, (1, 9, 7, 9));
}

BOOST_AUTO_TEST_CASE(assign_constant_value)
{
    compute::vector<float> device_vector(10, context);
    device_vector.assign(3, 6.28f, queue);
    BOOST_CHECK_EQUAL(device_vector.size(), size_t(3));
    CHECK_RANGE_EQUAL(float, 3, device_vector, (6.28f, 6.28f, 6.28f));
}

BOOST_AUTO_TEST_CASE(resize_throw_exception)
{
    // create vector with eight items
    int data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    compute::vector<int> vec(data, data + 8, queue);

    // try to resize to 2x larger than the global memory size
    BOOST_CHECK_THROW(
        vec.resize((device.global_memory_size() / sizeof(int)) * 2),
        boost::compute::opencl_error
    );

    // ensure vector data is still the same
    BOOST_CHECK_EQUAL(vec.size(), size_t(8));
    CHECK_RANGE_EQUAL(int, 8, vec, (1, 2, 3, 4, 5, 6, 7, 8));
}

BOOST_AUTO_TEST_CASE(copy_ctor_custom_alloc)
{
    int data[] = { 11, 12, 13, 14 };
    bc::vector<int, bc::pinned_allocator<int> > a(data, data + 4, queue);
    BOOST_CHECK_EQUAL(a.size(), size_t(4));
    CHECK_RANGE_EQUAL(int, 4, a, (11, 12, 13, 14));

    bc::vector<int, bc::pinned_allocator<int> > b(a, queue);
    BOOST_CHECK_EQUAL(b.size(), size_t(4));
    CHECK_RANGE_EQUAL(int, 4, b, (11, 12, 13, 14));
}

BOOST_AUTO_TEST_CASE(copy_ctor_different_alloc)
{
    int data[] = { 11, 12, 13, 14 };
    bc::vector<int> a(data, data + 4, queue);
    BOOST_CHECK_EQUAL(a.size(), size_t(4));
    CHECK_RANGE_EQUAL(int, 4, a, (11, 12, 13, 14));

    bc::vector<int, bc::pinned_allocator<int> > b(a, queue);
    BOOST_CHECK_EQUAL(b.size(), size_t(4));
    CHECK_RANGE_EQUAL(int, 4, b, (11, 12, 13, 14));

    std::vector<int> host_vector;
    host_vector.push_back(1);
    host_vector.push_back(9);
    host_vector.push_back(7);
    host_vector.push_back(9);

    bc::vector<int, bc::pinned_allocator<int> > c(host_vector, queue);
    BOOST_CHECK_EQUAL(c.size(), size_t(4));
    CHECK_RANGE_EQUAL(int, 4, c, (1, 9, 7, 9));
}

BOOST_AUTO_TEST_CASE(assignment_operator)
{
    int adata[] = { 11, 12, 13, 14 };
    bc::vector<int> a(adata, adata + 4, queue);
    BOOST_CHECK_EQUAL(a.size(), size_t(4));
    CHECK_RANGE_EQUAL(int, 4, a, (11, 12, 13, 14));

    bc::vector<int> b = a;
    BOOST_CHECK_EQUAL(b.size(), size_t(4));
    CHECK_RANGE_EQUAL(int, 4, b, (11, 12, 13, 14));

    bc::vector<int, bc::pinned_allocator<int> > c = b;
    BOOST_CHECK_EQUAL(c.size(), size_t(4));
    CHECK_RANGE_EQUAL(int, 4, c, (11, 12, 13, 14));

    int ddata[] = { 21, 22, 23 };
    bc::vector<int, bc::pinned_allocator<int> > d(ddata, ddata + 3, queue);
    BOOST_CHECK_EQUAL(d.size(), size_t(3));
    CHECK_RANGE_EQUAL(int, 3, d, (21, 22, 23));

    a = d;
    BOOST_CHECK_EQUAL(a.size(), size_t(3));
    CHECK_RANGE_EQUAL(int, 3, a, (21, 22, 23));

    std::vector<int> host_vector;
    host_vector.push_back(1);
    host_vector.push_back(9);
    host_vector.push_back(7);
    host_vector.push_back(9);

    d = host_vector;
    BOOST_CHECK_EQUAL(d.size(), size_t(4));
    CHECK_RANGE_EQUAL(int, 4, d, (1, 9, 7, 9));
}

BOOST_AUTO_TEST_CASE(swap_ctor_custom_alloc)
{
    int adata[] = { 11, 12, 13, 14 };
    bc::vector<int, bc::pinned_allocator<int> > a(adata, adata + 4, queue);
    BOOST_CHECK_EQUAL(a.size(), size_t(4));
    CHECK_RANGE_EQUAL(int, 4, a, (11, 12, 13, 14));

    int bdata[] = { 21, 22, 23 };
    bc::vector<int, bc::pinned_allocator<int> > b(bdata, bdata + 3, queue);
    BOOST_CHECK_EQUAL(b.size(), size_t(3));
    CHECK_RANGE_EQUAL(int, 3, b, (21, 22, 23));

    a.swap(b);
    BOOST_CHECK_EQUAL(a.size(), size_t(3));
    CHECK_RANGE_EQUAL(int, 3, a, (21, 22, 23));
    BOOST_CHECK_EQUAL(b.size(), size_t(4));
    CHECK_RANGE_EQUAL(int, 4, b, (11, 12, 13, 14));
}

BOOST_AUTO_TEST_SUITE_END()
