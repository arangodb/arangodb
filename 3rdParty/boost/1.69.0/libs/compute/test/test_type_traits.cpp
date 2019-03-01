//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestTypeTraits
#include <boost/test/unit_test.hpp>

#include <set>
#include <list>
#include <vector>

#include <boost/type_traits.hpp>
#include <boost/static_assert.hpp>

#include <boost/compute/types.hpp>
#include <boost/compute/type_traits.hpp>
#include <boost/compute/iterator/buffer_iterator.hpp>
#include <boost/compute/iterator/constant_iterator.hpp>
#include <boost/compute/detail/is_buffer_iterator.hpp>
#include <boost/compute/detail/is_contiguous_iterator.hpp>

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(scalar_type)
{
    BOOST_STATIC_ASSERT((boost::is_same<bc::scalar_type<bc::int_>::type, int>::value));
    BOOST_STATIC_ASSERT((boost::is_same<bc::scalar_type<bc::int2_>::type, int>::value));
    BOOST_STATIC_ASSERT((boost::is_same<bc::scalar_type<bc::float_>::type, float>::value));
    BOOST_STATIC_ASSERT((boost::is_same<bc::scalar_type<bc::float4_>::type, float>::value));
}

BOOST_AUTO_TEST_CASE(vector_size)
{
    BOOST_STATIC_ASSERT(bc::vector_size<bc::int_>::value == 1);
    BOOST_STATIC_ASSERT(bc::vector_size<bc::int2_>::value == 2);
    BOOST_STATIC_ASSERT(bc::vector_size<bc::float_>::value == 1);
    BOOST_STATIC_ASSERT(bc::vector_size<bc::float4_>::value == 4);
}

BOOST_AUTO_TEST_CASE(is_vector_type)
{
    BOOST_STATIC_ASSERT(bc::is_vector_type<bc::int_>::value == false);
    BOOST_STATIC_ASSERT(bc::is_vector_type<bc::int2_>::value == true);
    BOOST_STATIC_ASSERT(bc::is_vector_type<bc::float_>::value == false);
    BOOST_STATIC_ASSERT(bc::is_vector_type<bc::float4_>::value == true);
}

BOOST_AUTO_TEST_CASE(make_vector_type)
{
    BOOST_STATIC_ASSERT((boost::is_same<bc::make_vector_type<cl_uint, 2>::type, bc::uint2_>::value));
    BOOST_STATIC_ASSERT((boost::is_same<bc::make_vector_type<int, 4>::type, bc::int4_>::value));
    BOOST_STATIC_ASSERT((boost::is_same<bc::make_vector_type<float, 8>::type, bc::float8_>::value));
    BOOST_STATIC_ASSERT((boost::is_same<bc::make_vector_type<bc::char_, 16>::type, bc::char16_>::value));
}

BOOST_AUTO_TEST_CASE(is_fundamental_type)
{
    BOOST_STATIC_ASSERT((bc::is_fundamental<int>::value == true));
    BOOST_STATIC_ASSERT((bc::is_fundamental<bc::int_>::value == true));
    BOOST_STATIC_ASSERT((bc::is_fundamental<bc::int2_>::value == true));
    BOOST_STATIC_ASSERT((bc::is_fundamental<float>::value == true));
    BOOST_STATIC_ASSERT((bc::is_fundamental<bc::float_>::value == true));
    BOOST_STATIC_ASSERT((bc::is_fundamental<bc::float4_>::value == true));

    BOOST_STATIC_ASSERT((bc::is_fundamental<std::pair<int, float> >::value == false));
    BOOST_STATIC_ASSERT((bc::is_fundamental<std::complex<float> >::value == false));
}

BOOST_AUTO_TEST_CASE(type_name)
{
    // scalar types
    BOOST_CHECK(std::strcmp(bc::type_name<bc::char_>(), "char") == 0);
    BOOST_CHECK(std::strcmp(bc::type_name<bc::uchar_>(), "uchar") == 0);
    BOOST_CHECK(std::strcmp(bc::type_name<bc::short_>(), "short") == 0);
    BOOST_CHECK(std::strcmp(bc::type_name<bc::ushort_>(), "ushort") == 0);
    BOOST_CHECK(std::strcmp(bc::type_name<bc::int_>(), "int") == 0);
    BOOST_CHECK(std::strcmp(bc::type_name<bc::uint_>(), "uint") == 0);
    BOOST_CHECK(std::strcmp(bc::type_name<bc::long_>(), "long") == 0);
    BOOST_CHECK(std::strcmp(bc::type_name<bc::ulong_>(), "ulong") == 0);
    BOOST_CHECK(std::strcmp(bc::type_name<bc::float_>(), "float") == 0);
    BOOST_CHECK(std::strcmp(bc::type_name<bc::double_>(), "double") == 0);
    BOOST_CHECK(std::strcmp(bc::type_name<bool>(), "bool") == 0);

    // vector types
    BOOST_CHECK(std::strcmp(bc::type_name<bc::char16_>(), "char16") == 0);
    BOOST_CHECK(std::strcmp(bc::type_name<bc::uint4_>(), "uint4") == 0);
    BOOST_CHECK(std::strcmp(bc::type_name<bc::ulong8_>(), "ulong8") == 0);
    BOOST_CHECK(std::strcmp(bc::type_name<bc::float2_>(), "float2") == 0);
    BOOST_CHECK(std::strcmp(bc::type_name<bc::double4_>(), "double4") == 0);
}

BOOST_AUTO_TEST_CASE(is_contiguous_iterator)
{
    using boost::compute::detail::is_contiguous_iterator;

    BOOST_STATIC_ASSERT(is_contiguous_iterator<int *>::value == true);
    BOOST_STATIC_ASSERT(is_contiguous_iterator<std::vector<int>::iterator>::value == true);
    BOOST_STATIC_ASSERT(is_contiguous_iterator<std::vector<int>::const_iterator>::value == true);
    BOOST_STATIC_ASSERT(is_contiguous_iterator<std::list<int>::iterator>::value == false);
    BOOST_STATIC_ASSERT(is_contiguous_iterator<std::set<int>::iterator>::value == false);
    BOOST_STATIC_ASSERT(is_contiguous_iterator<std::insert_iterator<std::set<int> > >::value == false);
    BOOST_STATIC_ASSERT(is_contiguous_iterator<std::back_insert_iterator<std::vector<int> > >::value == false);
}

BOOST_AUTO_TEST_CASE(is_buffer_iterator)
{
    using boost::compute::detail::is_buffer_iterator;

    BOOST_STATIC_ASSERT(is_buffer_iterator<boost::compute::buffer_iterator<int> >::value == true);
    BOOST_STATIC_ASSERT(is_buffer_iterator<boost::compute::constant_iterator<int> >::value == false);
}

BOOST_AUTO_TEST_CASE(is_device_iterator)
{
    using boost::compute::is_device_iterator;

    BOOST_STATIC_ASSERT(is_device_iterator<boost::compute::buffer_iterator<int> >::value == true);
    BOOST_STATIC_ASSERT(is_device_iterator<const boost::compute::buffer_iterator<int> >::value == true);
    BOOST_STATIC_ASSERT(is_device_iterator<boost::compute::constant_iterator<int> >::value == true);
    BOOST_STATIC_ASSERT(is_device_iterator<const boost::compute::constant_iterator<int> >::value == true);
    BOOST_STATIC_ASSERT(is_device_iterator<float *>::value == false);
    BOOST_STATIC_ASSERT(is_device_iterator<const float *>::value == false);
    BOOST_STATIC_ASSERT(is_device_iterator<std::vector<int>::iterator>::value == false);
    BOOST_STATIC_ASSERT(is_device_iterator<const std::vector<int>::iterator>::value == false);
}
