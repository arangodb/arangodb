//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/pixel.hpp>
#include <boost/gil/planar_pixel_iterator.hpp>
#include <boost/gil/typedefs.hpp>

#include <boost/mp11.hpp>

namespace gil = boost::gil;
using namespace boost::mp11;

int main()
{
    using iterators = mp_list
    <
        gil::planar_pixel_iterator<gil::rgb8_view_t*, gil::rgb_t>,
        gil::rgb8_planar_ptr_t,
        gil::rgb8_planar_step_ptr_t,
        gil::rgb8c_planar_ptr_t,
        gil::rgb8c_planar_step_ptr_t,
        gil::rgb16_planar_ptr_t,
        gil::rgb16c_planar_ptr_t,
        gil::rgb16s_planar_ptr_t,
        gil::rgb16sc_planar_ptr_t,
        gil::rgb32_planar_ptr_t,
        gil::rgb32c_planar_ptr_t,
        gil::rgb32f_planar_ptr_t,
        gil::rgb32fc_planar_ptr_t,
        gil::rgb32s_planar_ptr_t,
        gil::rgb32sc_planar_ptr_t,
        gil::cmyk8_planar_ptr_t,
        gil::cmyk8c_planar_ptr_t,
        gil::cmyk8s_planar_ptr_t,
        gil::cmyk8sc_planar_ptr_t,
        gil::cmyk16_planar_ptr_t,
        gil::cmyk16c_planar_ptr_t,
        gil::cmyk16s_planar_ptr_t,
        gil::cmyk16sc_planar_ptr_t,
        gil::cmyk32_planar_ptr_t,
        gil::cmyk32c_planar_ptr_t,
        gil::cmyk32f_planar_ptr_t,
        gil::cmyk32fc_planar_ptr_t,
        gil::cmyk32s_planar_ptr_t,
        gil::cmyk32sc_planar_ptr_t,
        gil::rgba8_planar_ptr_t,
        gil::rgba8c_planar_ptr_t,
        gil::rgba8s_planar_ptr_t,
        gil::rgba8sc_planar_ptr_t,
        gil::rgba16_planar_ptr_t,
        gil::rgba16c_planar_ptr_t,
        gil::rgba16s_planar_ptr_t,
        gil::rgba16sc_planar_ptr_t,
        gil::rgba32_planar_ptr_t,
        gil::rgba32c_planar_ptr_t,
        gil::rgba32f_planar_ptr_t,
        gil::rgba32fc_planar_ptr_t,
        gil::rgba32s_planar_ptr_t,
        gil::rgba32sc_planar_ptr_t
    >;

    static_assert(std::is_same
        <
            mp_all_of<iterators, gil::is_planar>,
            std::true_type
        >::value,
        "is_planar yields true for non-planar pixel iterator");
}
