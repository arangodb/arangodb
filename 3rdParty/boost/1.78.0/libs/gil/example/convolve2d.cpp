#include <vector>
#include <iostream>
#include <boost/gil/extension/numeric/kernel.hpp>
#include <boost/gil/extension/numeric/convolve.hpp>
#include <boost/gil/extension/io/png.hpp>

#include <boost/gil/extension/io/jpeg.hpp>
using namespace boost::gil;
using namespace std;
int main()
{
    //gray8_image_t img;
    //read_image("test_adaptive.png", img, png_tag{});
    //gray8_image_t img_out(img.dimensions());

    gray8_image_t img;
    read_image("src_view.png", img, png_tag{});
    gray8_image_t img_out(img.dimensions()), img_out1(img.dimensions());

    std::vector<float> v(9, 1.0f / 9.0f);
    detail::kernel_2d<float> kernel(v.begin(), v.size(), 1, 1);
    detail::convolve_2d(view(img), kernel, view(img_out1));

    //write_view("out-convolve2d.png", view(img_out), png_tag{});
    write_view("out-convolve2d.png", view(img_out1), jpeg_tag{});


    //------------------------------------//
    std::vector<float> v1(3, 1.0f / 3.0f);
    kernel_1d<float> kernel1(v1.begin(), v1.size(), 1);

    detail::convolve_1d<gray32f_pixel_t>(const_view(img), kernel1, view(img_out), boundary_option::extend_zero);
    write_view("out-convolve_option_extend_zero.png", view(img_out), png_tag{});

    if (equal_pixels(view(img_out1), view(img_out)))cout << "convolve_option_extend_zero" << endl;

    cout << "done\n";
    cin.get();

    return 0;
}
