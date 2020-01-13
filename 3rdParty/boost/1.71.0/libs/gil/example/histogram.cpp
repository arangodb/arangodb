//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>
#include <boost/gil/extension/io/jpeg.hpp>

#include <algorithm>
#include <fstream>

// Example file to demonstrate a way to compute histogram

using namespace boost::gil;

template <typename GrayView, typename R>
void gray_image_hist(GrayView const& img_view, R& hist)
{
    for (auto it = img_view.begin(); it != img_view.end(); ++it)
        ++hist[*it];

    // Alternatively, prefer the algorithm with lambda
    // for_each_pixel(img_view, [&hist](gray8_pixel_t const& pixel) {
    //     ++hist[pixel];
    // });
}

template <typename V, typename R>
void get_hist(const V& img_view, R& hist) {
    gray_image_hist(color_converted_view<gray8_pixel_t>(img_view), hist);
}

int main() {
    rgb8_image_t img;
    read_image("test.jpg", img, jpeg_tag());

    int histogram[256];
    std::fill(histogram,histogram + 256, 0);
    get_hist(const_view(img), histogram);

    std::fstream histo_file("out-histogram.txt", std::ios::out);
    for(std::size_t ii = 0; ii < 256; ++ii)
        histo_file << histogram[ii] << std::endl;
    histo_file.close();

    return 0;
}
