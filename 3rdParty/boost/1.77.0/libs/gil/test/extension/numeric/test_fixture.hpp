//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/numeric/kernel.hpp>
#include <boost/assert.hpp>

#include <initializer_list>
#include <type_traits>

namespace boost { namespace gil {

namespace test { namespace fixture {

template <typename T>
auto create_kernel(std::initializer_list<T> const& values)
    -> gil::kernel_1d<T>
{
    static_assert(std::is_arithmetic<T>::value,
        "kernel value type should be integral or floating-point type");
    BOOST_ASSERT_MSG((values.size() - 1) % 2 == 0, "expected odd number of kernel values");
    gil::kernel_1d<T> kernel(values.begin(), values.size(), (values.size() - 1) / 2);
    return kernel;
}

}}}} // namespace boost::gil::test::fixture
