//---------------------------------------------------------------------------//
// Copyright (c) 2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <iostream>
#include <algorithm>

#include <QtGlobal>
#if QT_VERSION >= 0x050000
#include <QtWidgets>
#else
#include <QtGui>
#endif

#ifndef Q_MOC_RUN
#include <boost/compute/system.hpp>
#include <boost/compute/image/image2d.hpp>
#include <boost/compute/interop/qt.hpp>
#include <boost/compute/utility/dim.hpp>
#include <boost/compute/utility/source.hpp>
#endif // Q_MOC_RUN

namespace compute = boost::compute;

inline void box_filter_image(const compute::image2d &input,
                             compute::image2d &output,
                             compute::uint_ box_height,
                             compute::uint_ box_width,
                             compute::command_queue &queue)
{
    using compute::dim;

    const compute::context &context = queue.get_context();

    // simple box filter kernel source
    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void box_filter(__read_only image2d_t input,
                                 __write_only image2d_t output,
                                 uint box_height,
                                 uint box_width)
        {
            int x = get_global_id(0);
            int y = get_global_id(1);
            int h = get_image_height(input);
            int w = get_image_width(input);
            int k = box_width;
            int l = box_height;

            if(x < k/2 || y < l/2 || x >= w-(k/2) || y >= h-(l/2)){
                write_imagef(output, (int2)(x, y), (float4)(0, 0, 0, 1));
            }
            else {
                const sampler_t sampler = CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;

                float4 sum = { 0, 0, 0, 0 };
                for(int i = 0; i < k; i++){
                    for(int j = 0; j < l; j++){
                        sum += read_imagef(input, sampler, (int2)(x+i-k, y+j-l));
                    }
                }
                sum /= (float) k * l;
                float4 value = (float4)( sum.x, sum.y, sum.z, 1.f );
                write_imagef(output, (int2)(x, y), value);
            }
        }
    );

    // build box filter program
    compute::program program =
        compute::program::create_with_source(source, context);
    program.build();

    // setup box filter kernel
    compute::kernel kernel(program, "box_filter");
    kernel.set_arg(0, input);
    kernel.set_arg(1, output);
    kernel.set_arg(2, box_height);
    kernel.set_arg(3, box_width);

    // execute the box filter kernel
    queue.enqueue_nd_range_kernel(kernel, dim(0, 0), input.size(), dim(1, 1));
}

// this example shows how to load an image using Qt, apply a simple
// box blur filter, and then display it in a Qt window.
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // check command line
    if(argc < 2){
        std::cout << "usage: qimage_blur [FILENAME]" << std::endl;
        return -1;
    }

    // load image using Qt
    QString fileName = argv[1];
    QImage qimage(fileName);

    size_t height = qimage.height();
    size_t width = qimage.width();
    size_t bytes_per_line = qimage.bytesPerLine();

    qDebug() << "height:" << height
             << "width:" << width
             << "bytes per line:" << bytes_per_line
             << "depth:" << qimage.depth()
             << "format:" << qimage.format();

    // create compute context
    compute::device gpu = compute::system::default_device();
    compute::context context(gpu);
    compute::command_queue queue(context, gpu);
    std::cout << "device: " << gpu.name() << std::endl;

    // get the opencl image format for the qimage
    compute::image_format format =
        compute::qt_qimage_format_to_image_format(qimage.format());

    // create input and output images on the gpu
    compute::image2d input_image(context, width, height, format);
    compute::image2d output_image(context, width, height, format);

    // copy host qimage to gpu image
    compute::qt_copy_qimage_to_image2d(qimage, input_image, queue);

    // apply box filter
    box_filter_image(input_image, output_image, 7, 7, queue);

    // copy gpu blurred image from to host qimage
    compute::qt_copy_image2d_to_qimage(output_image, qimage, queue);

    // show image as a pixmap
    QLabel label;
    label.setPixmap(QPixmap::fromImage(qimage));
    label.show();

    return app.exec();
}
