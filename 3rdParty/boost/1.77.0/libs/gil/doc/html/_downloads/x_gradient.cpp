//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/extension/io/jpeg.hpp>

// Example to demonstrate a way to compute gradients along x-axis

using namespace boost::gil;

template <typename Out>
struct halfdiff_cast_channels {
    template <typename T> Out operator()(const T& in1, const T& in2) const {
        return Out((in2-in1)/2);
    }
};


template <typename SrcView, typename DstView>
void x_gradient(SrcView const& src, DstView const& dst)
{
    using dst_channel_t = typename channel_type<DstView>::type;

    for (int y = 0; y < src.height(); ++y)
    {
        typename SrcView::x_iterator src_it = src.row_begin(y);
        typename DstView::x_iterator dst_it = dst.row_begin(y);

        for (int x = 1; x < src.width() - 1; ++x)
        {
            static_transform(src_it[x - 1], src_it[x + 1], dst_it[x],
                halfdiff_cast_channels<dst_channel_t>());
        }
    }
}

template <typename SrcView, typename DstView>
void x_luminosity_gradient(SrcView const& src, DstView const& dst)
{
    using gray_pixel_t = pixel<typename channel_type<SrcView>::type, gray_layout_t>;
    x_gradient(color_converted_view<gray_pixel_t>(src), dst);
}

int main()
{
    rgb8_image_t img;
    read_image("test.jpg",img, jpeg_tag{});

    gray8s_image_t img_out(img.dimensions());
    fill_pixels(view(img_out),int8_t(0));

    x_luminosity_gradient(const_view(img), view(img_out));
    write_view("out-x_gradient.jpg",color_converted_view<gray8_pixel_t>(const_view(img_out)), jpeg_tag{});

    return 0;
}
