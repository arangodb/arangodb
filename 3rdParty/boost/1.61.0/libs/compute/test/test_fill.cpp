//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestFill
#include <boost/test/unit_test.hpp>
#include <boost/test/test_case_template.hpp>
#include <boost/mpl/list.hpp>

#include <boost/compute/algorithm/equal.hpp>
#include <boost/compute/algorithm/fill.hpp>
#include <boost/compute/algorithm/fill_n.hpp>
#include <boost/compute/async/future.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/svm.hpp>
#include <boost/compute/type_traits.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

typedef boost::mpl::list
        <bc::char_, bc::uchar_, bc::int_, bc::uint_,
         bc::long_, bc::ulong_, bc::float_, bc::double_>
        scalar_types;

template<class T>
inline void test_fill(T v1, T v2, T v3, bc::command_queue queue) {
    if(boost::is_same<typename bc::scalar_type<T>::type, bc::double_>::value &&
       !queue.get_device().supports_extension("cl_khr_fp64")) {
        std::cerr << "Skipping test_fill<" << bc::type_name<T>() << ">() "
                     "on device which doesn't support cl_khr_fp64" << std::endl;
        return;
    }

    bc::vector<T> vector(4, queue.get_context());
    bc::fill(vector.begin(), vector.end(), v1, queue);
    queue.finish();
    CHECK_RANGE_EQUAL(T, 4, vector, (v1, v1, v1, v1));

    vector.resize(1000, queue);
    bc::fill(vector.begin(), vector.end(), v2, queue);
    queue.finish();
    BOOST_CHECK_EQUAL(vector.front(), v2);
    BOOST_CHECK_EQUAL(vector.back(), v2);

    bc::fill(vector.begin() + 500, vector.end(), v3, queue);
    queue.finish();
    BOOST_CHECK_EQUAL(vector.front(), v2);
    BOOST_CHECK_EQUAL(vector[499], v2);
    BOOST_CHECK_EQUAL(vector[500], v3);
    BOOST_CHECK_EQUAL(vector.back(), v3);
}

BOOST_AUTO_TEST_CASE_TEMPLATE( fill_scalar, S, scalar_types )
{
    S v1 = S(1.5f);
    S v2 = S(2.5f);
    S v3 = S(42.0f);
    test_fill(v1, v2, v3, queue);
}

BOOST_AUTO_TEST_CASE_TEMPLATE( fill_vec2, S, scalar_types )
{
    typedef typename bc::make_vector_type<S, 2>::type T;
    S s1 = S(1.5f);
    S s2 = S(2.5f);
    S s3 = S(42.0f);
    S s4 = S(84.0f);

    T v1 = T(s1, s2);
    T v2 = T(s3, s4);
    T v3 = T(s2, s1);
    test_fill(v1, v2, v3, queue);
}

BOOST_AUTO_TEST_CASE_TEMPLATE( fill_vec4, S, scalar_types )
{
    typedef typename bc::make_vector_type<S, 4>::type T;
    S s1 = S(1.5f);
    S s2 = S(2.5f);
    S s3 = S(42.0f);
    S s4 = S(84.0f);

    T v1 = T(s1, s2, s3, s4);
    T v2 = T(s3, s4, s1, s2);
    T v3 = T(s4, s3, s2, s1);
    test_fill(v1, v2, v3, queue);
}

BOOST_AUTO_TEST_CASE_TEMPLATE( fill_vec8, S, scalar_types )
{
    typedef typename bc::make_vector_type<S, 8>::type T;
    S s1 = S(1.5f);
    S s2 = S(2.5f);
    S s3 = S(42.0f);
    S s4 = S(84.0f);
    S s5 = S(122.5f);
    S s6 = S(131.5f);
    S s7 = S(142.0f);
    S s8 = S(254.0f);

    T v1 = T(s1, s2, s3, s4, s5, s6, s7, s8);
    T v2 = T(s3, s4, s1, s2, s7, s8, s5, s6);
    T v3 = T(s4, s3, s2, s1, s8, s7, s6, s5);
    test_fill(v1, v2, v3, queue);
}

BOOST_AUTO_TEST_CASE_TEMPLATE( fill_vec16, S, scalar_types )
{
    typedef typename bc::make_vector_type<S, 16>::type T;
    S s1 = S(1.5f);
    S s2 = S(2.5f);
    S s3 = S(42.0f);
    S s4 = S(84.0f);
    S s5 = S(122.5f);
    S s6 = S(131.5f);
    S s7 = S(142.0f);
    S s8 = S(254.0f);

    T v1 = T(s1, s2, s3, s4, s5, s6, s7, s8, s1, s2, s3, s4, s5, s6, s7, s8);
    T v2 = T(s3, s4, s1, s2, s7, s8, s5, s6, s4, s3, s2, s1, s8, s7, s6, s5);
    T v3 = T(s4, s3, s2, s1, s8, s7, s6, s5, s8, s7, s6, s5, s4, s3, s2, s1);
    test_fill(v1, v2, v3, queue);
}

template<class T>
inline void test_fill_n(T v1, T v2, T v3, bc::command_queue queue) {
    if(boost::is_same<typename bc::scalar_type<T>::type, bc::double_>::value &&
       !queue.get_device().supports_extension("cl_khr_fp64")) {
        std::cerr << "Skipping test_fill_n<" << bc::type_name<T>() << ">() "
                     "on device which doesn't support cl_khr_fp64" << std::endl;
        return;
    }

    bc::vector<T> vector(4, queue.get_context());
    bc::fill_n(vector.begin(), 4, v1, queue);
    queue.finish();
    CHECK_RANGE_EQUAL(T, 4, vector, (v1, v1, v1, v1));

    bc::fill_n(vector.begin(), 3, v2, queue);
    queue.finish();
    CHECK_RANGE_EQUAL(T, 4, vector, (v2, v2, v2, v1));

    bc::fill_n(vector.begin() + 1, 2, v3, queue);
    queue.finish();
    CHECK_RANGE_EQUAL(T, 4, vector, (v2, v3, v3, v1));

    bc::fill_n(vector.begin(), 4, v2, queue);
    queue.finish();
    CHECK_RANGE_EQUAL(T, 4, vector, (v2, v2, v2, v2));

    // fill last element
    bc::fill_n(vector.end() - 1, 1, v3, queue);
    queue.finish();
    CHECK_RANGE_EQUAL(T, 4, vector, (v2, v2, v2, v3));

    // fill first element
    bc::fill_n(vector.begin(), 1, v1, queue);
    queue.finish();
    CHECK_RANGE_EQUAL(T, 4, vector, (v1, v2, v2, v3));
}

BOOST_AUTO_TEST_CASE_TEMPLATE( fill_n_scalar, S, scalar_types )
{
    S v1 = S(1.5f);
    S v2 = S(2.5f);
    S v3 = S(42.0f);
    test_fill_n(v1, v2, v3, queue);
}

BOOST_AUTO_TEST_CASE_TEMPLATE( fill_n_vec2, S, scalar_types )
{
    typedef typename bc::make_vector_type<S, 2>::type T;
    S s1 = S(1.5f);
    S s2 = S(2.5f);
    S s3 = S(42.0f);
    S s4 = S(84.0f);

    T v1 = T(s1, s2);
    T v2 = T(s3, s4);
    T v3 = T(s2, s1);
    test_fill_n(v1, v2, v3, queue);
}

BOOST_AUTO_TEST_CASE_TEMPLATE( fill_n_vec4, S, scalar_types )
{
    typedef typename bc::make_vector_type<S, 4>::type T;
    S s1 = S(1.5f);
    S s2 = S(2.5f);
    S s3 = S(42.0f);
    S s4 = S(84.0f);

    T v1 = T(s1, s2, s3, s4);
    T v2 = T(s3, s4, s1, s2);
    T v3 = T(s4, s3, s2, s1);
    test_fill_n(v1, v2, v3, queue);
}

BOOST_AUTO_TEST_CASE_TEMPLATE( fill_n_vec8, S, scalar_types )
{
    typedef typename bc::make_vector_type<S, 8>::type T;
    S s1 = S(1.5f);
    S s2 = S(2.5f);
    S s3 = S(42.0f);
    S s4 = S(84.0f);
    S s5 = S(122.5f);
    S s6 = S(131.5f);
    S s7 = S(142.0f);
    S s8 = S(254.0f);

    T v1 = T(s1, s2, s3, s4, s5, s6, s7, s8);
    T v2 = T(s3, s4, s1, s2, s7, s8, s5, s6);
    T v3 = T(s4, s3, s2, s1, s8, s7, s6, s5);
    test_fill_n(v1, v2, v3, queue);
}

BOOST_AUTO_TEST_CASE_TEMPLATE( fill_n_vec16, S, scalar_types )
{
    typedef typename bc::make_vector_type<S, 16>::type T;
    S s1 = S(1.5f);
    S s2 = S(2.5f);
    S s3 = S(42.0f);
    S s4 = S(84.0f);
    S s5 = S(122.5f);
    S s6 = S(131.5f);
    S s7 = S(142.0f);
    S s8 = S(254.0f);

    T v1 = T(s1, s2, s3, s4, s5, s6, s7, s8, s1, s2, s3, s4, s5, s6, s7, s8);
    T v2 = T(s3, s4, s1, s2, s7, s8, s5, s6, s4, s3, s2, s1, s8, s7, s6, s5);
    T v3 = T(s4, s3, s2, s1, s8, s7, s6, s5, s8, s7, s6, s5, s4, s3, s2, s1);
    test_fill_n(v1, v2, v3, queue);
}

BOOST_AUTO_TEST_CASE(check_fill_type)
{
    bc::vector<int> vector(5, context);
    bc::future<void> future =
        bc::fill_async(vector.begin(), vector.end(), 42, queue);
    future.wait();

    #ifdef CL_VERSION_1_2
    BOOST_CHECK_EQUAL(
            future.get_event().get_command_type(),
            device.check_version(1,2) ? CL_COMMAND_FILL_BUFFER : CL_COMMAND_NDRANGE_KERNEL
            );
    #else
    BOOST_CHECK(
        future.get_event().get_command_type() == CL_COMMAND_NDRANGE_KERNEL
    );
    #endif
}

BOOST_AUTO_TEST_CASE(fill_clone_buffer)
{
    int data[] = { 1, 2, 3, 4 };
    bc::vector<int> vec(data, data + 4, queue);
    CHECK_RANGE_EQUAL(int, 4, vec, (1, 2, 3, 4));

    bc::buffer cloned_buffer = vec.get_buffer().clone(queue);
    BOOST_CHECK(
        bc::equal(
            vec.begin(),
            vec.end(),
            bc::make_buffer_iterator<int>(cloned_buffer, 0),
            queue
        )
    );

    bc::fill(vec.begin(), vec.end(), 5, queue);
    BOOST_CHECK(
        !bc::equal(
            vec.begin(),
            vec.end(),
            bc::make_buffer_iterator<int>(cloned_buffer, 0),
            queue
        )
    );

    bc::fill(
        bc::make_buffer_iterator<int>(cloned_buffer, 0),
        bc::make_buffer_iterator<int>(cloned_buffer, 4),
        5,
        queue
    );
    BOOST_CHECK(
        bc::equal(
            vec.begin(),
            vec.end(),
            bc::make_buffer_iterator<int>(cloned_buffer, 0),
            queue
        )
    );
}

#ifdef CL_VERSION_2_0
BOOST_AUTO_TEST_CASE(fill_svm_buffer)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    size_t size = 4;
    bc::svm_ptr<cl_int> ptr =
        bc::svm_alloc<cl_int>(context, size * sizeof(cl_int));
    bc::fill_n(ptr, size * sizeof(cl_int), 42, queue);

    queue.enqueue_svm_map(ptr.get(), size * sizeof(cl_int), CL_MAP_READ);
    for(size_t i = 0; i < size; i++) {
        BOOST_CHECK_EQUAL(static_cast<cl_int*>(ptr.get())[i], 42);
    }
    queue.enqueue_svm_unmap(ptr.get());

    bc::svm_free(context, ptr);
}
#endif // CL_VERSION_2_0

BOOST_AUTO_TEST_CASE(empty_fill)
{
    bc::vector<int> vec(0, context);
    bc::fill(vec.begin(), vec.end(), 42, queue);
    bc::fill_async(vec.begin(), vec.end(), 42, queue);
}

BOOST_AUTO_TEST_SUITE_END()
