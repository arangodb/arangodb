//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/mp11.hpp>

#include "test_fixture.hpp"

namespace gil = boost::gil;
using namespace boost::mp11;

int main()
{
    static_assert(mp_all_of
        <
            gil::test::fixture::pixel_typedefs,
            gil::pixel_reference_is_mutable
        >::value,
        "pixel_reference_is_mutable should yield true for all core pixel typedefs");

    static_assert(!mp_all_of
        <
            mp_transform
            <
                gil::test::fixture::nested_type,
                gil::test::fixture::representative_pixel_types
            >,
            gil::pixel_reference_is_mutable
        >::value,
        "pixel_reference_is_mutable should yield true for some representative core pixel types");
}
