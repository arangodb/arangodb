//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
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
#include <QtOpenGL>

#include <boost/program_options.hpp>

#ifndef Q_MOC_RUN
#include <boost/compute/command_queue.hpp>
#include <boost/compute/kernel.hpp>
#include <boost/compute/program.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/image/image2d.hpp>
#include <boost/compute/image/image_sampler.hpp>
#include <boost/compute/interop/qt.hpp>
#include <boost/compute/interop/opengl.hpp>
#include <boost/compute/utility/source.hpp>
#endif // Q_MOC_RUN

namespace compute = boost::compute;
namespace po = boost::program_options;

// opencl source code
const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
    __kernel void resize_image(__read_only image2d_t input,
                               const sampler_t sampler,
                               __write_only image2d_t output)
    {
        const uint x = get_global_id(0);
        const uint y = get_global_id(1);

        const float w = get_image_width(output);
        const float h = get_image_height(output);

        float2 coord = { ((float) x / w) * get_image_width(input),
                         ((float) y / h) * get_image_height(input) };

        float4 pixel = read_imagef(input, sampler, coord);
        write_imagef(output, (int2)(x, h - y - 1), pixel);
    };
);

class ImageWidget : public QGLWidget
{
    Q_OBJECT

public:
    ImageWidget(QString fileName, QWidget *parent = 0);
    ~ImageWidget();

    void initializeGL();
    void resizeGL(int width, int height);
    void paintGL();

private:
    QImage qt_image_;
    compute::context context_;
    compute::command_queue queue_;
    compute::program program_;
    compute::image2d image_;
    compute::image_sampler sampler_;
    GLuint gl_texture_;
    compute::opengl_texture cl_texture_;
};

ImageWidget::ImageWidget(QString fileName, QWidget *parent)
    : QGLWidget(parent),
      qt_image_(fileName)
{
    gl_texture_ = 0;
}

ImageWidget::~ImageWidget()
{
}

void ImageWidget::initializeGL()
{
    // setup opengl
    glDisable(GL_LIGHTING);

    // create the OpenGL/OpenCL shared context
    context_ = compute::opengl_create_shared_context();

    // get gpu device
    compute::device gpu = context_.get_device();
    std::cout << "device: " << gpu.name() << std::endl;

    // setup command queue
    queue_ = compute::command_queue(context_, gpu);

    // allocate image on the device
    compute::image_format format =
        compute::qt_qimage_format_to_image_format(qt_image_.format());

    image_ = compute::image2d(
        context_, qt_image_.width(), qt_image_.height(), format, CL_MEM_READ_ONLY
    );

    // transfer image to the device
    compute::qt_copy_qimage_to_image2d(qt_image_, image_, queue_);

    // setup image sampler (use CL_FILTER_NEAREST to disable linear interpolation)
    sampler_ = compute::image_sampler(
        context_, false, CL_ADDRESS_NONE, CL_FILTER_LINEAR
    );

    // build resize program
    program_ = compute::program::build_with_source(source, context_);
}

void ImageWidget::resizeGL(int width, int height)
{
#if QT_VERSION >= 0x050000
    // scale height/width based on device pixel ratio
    width /= windowHandle()->devicePixelRatio();
    height /= windowHandle()->devicePixelRatio();
#endif

    // resize viewport
    glViewport(0, 0, width, height);

    // delete old texture
    if(gl_texture_){
      glDeleteTextures(1, &gl_texture_);
      gl_texture_ = 0;
    }

    // generate new texture
    glGenTextures(1, &gl_texture_);
    glBindTexture(GL_TEXTURE_2D, gl_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0
    );

    // create opencl object for the texture
    cl_texture_ = compute::opengl_texture(
        context_, GL_TEXTURE_2D, 0, gl_texture_, CL_MEM_WRITE_ONLY
    );
}

void ImageWidget::paintGL()
{
    float w = width();
    float h = height();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, w, 0.0, h, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // setup the resize kernel
    compute::kernel kernel(program_, "resize_image");
    kernel.set_arg(0, image_);
    kernel.set_arg(1, sampler_);
    kernel.set_arg(2, cl_texture_);

    // acquire the opengl texture so it can be used in opencl
    compute::opengl_enqueue_acquire_gl_objects(1, &cl_texture_.get(), queue_);

    // execute the resize kernel
    const size_t global_work_offset[] = { 0, 0 };
    const size_t global_work_size[] = { size_t(width()), size_t(height()) };

    queue_.enqueue_nd_range_kernel(
        kernel, 2, global_work_offset, global_work_size, 0
    );

    // release the opengl texture so it can be used by opengl
    compute::opengl_enqueue_release_gl_objects(1, &cl_texture_.get(), queue_);

    // ensure opencl is finished before rendering in opengl
    queue_.finish();

    // draw a single quad with the resized image texture
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gl_texture_);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(0, 0);
    glTexCoord2f(0, 1); glVertex2f(0, h);
    glTexCoord2f(1, 1); glVertex2f(w, h);
    glTexCoord2f(1, 0); glVertex2f(w, 0);
    glEnd();
}

// the resize image example demonstrates how to interactively resize a
// 2D image and display it using OpenGL. a image sampler is used to perform
// hardware-accelerated linear interpolation for the resized image.
int main(int argc, char *argv[])
{
    // setup command line arguments
    po::options_description options("options");
    options.add_options()
        ("help", "show usage instructions")
        ("file", po::value<std::string>(), "image file name (e.g. /path/to/image.png)")
    ;
    po::positional_options_description positional_options;
    positional_options.add("file", 1);

    // parse command line
    po::variables_map vm;
    po::store(
        po::command_line_parser(argc, argv)
            .options(options)
            .positional(positional_options)
            .run(),
        vm
    );
    po::notify(vm);

    // check for file argument
    if(vm.count("help") || !vm.count("file")){
        std::cout << options << std::endl;
        return -1;
    }

    // get file name
    std::string file_name = vm["file"].as<std::string>();

    // setup qt application
    QApplication app(argc, argv);

    // setup image widget
    ImageWidget widget(QString::fromStdString(file_name));
    widget.show();

    // run qt application
    return app.exec();
}

#include "resize_image.moc"
