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

#ifndef Q_MOC_RUN
#include <boost/compute/command_queue.hpp>
#include <boost/compute/kernel.hpp>
#include <boost/compute/program.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/interop/opengl.hpp>
#include <boost/compute/utility/dim.hpp>
#include <boost/compute/utility/source.hpp>
#endif // Q_MOC_RUN

namespace compute = boost::compute;

// opencl source code
const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
    // map value to color
    float4 color(uint i)
    {
        uchar c = i;
        uchar x = 35;
        uchar y = 25;
        uchar z = 15;
        uchar max = 255;

        if(i == 256)
            return (float4)(0, 0, 0, 255);
        else
            return (float4)(max-x*i, max-y*i, max-z*i, max) / 255.f;
    }

    __kernel void mandelbrot(__write_only image2d_t image)
    {
        const uint x_coord = get_global_id(0);
        const uint y_coord = get_global_id(1);
        const uint width = get_global_size(0);
        const uint height = get_global_size(1);

        float x_origin = ((float) x_coord / width) * 3.25f - 2.0f;
        float y_origin = ((float) y_coord / height) * 2.5f - 1.25f;

        float x = 0.0f;
        float y = 0.0f;

        uint i = 0;
        while(x*x + y*y <= 4.f && i < 256){
            float tmp = x*x - y*y + x_origin;
            y = 2*x*y + y_origin;
            x = tmp;
            i++;
        }

        int2 coord = { x_coord, y_coord };
        write_imagef(image, coord, color(i));
    };
);

class MandelbrotWidget : public QGLWidget
{
    Q_OBJECT

public:
    MandelbrotWidget(QWidget *parent = 0);
    ~MandelbrotWidget();

    void initializeGL();
    void resizeGL(int width, int height);
    void paintGL();
    void keyPressEvent(QKeyEvent* event);

private:
    compute::context context_;
    compute::command_queue queue_;
    compute::program program_;
    GLuint gl_texture_;
    compute::opengl_texture cl_texture_;
};

MandelbrotWidget::MandelbrotWidget(QWidget *parent)
    : QGLWidget(parent)
{
    gl_texture_ = 0;
}

MandelbrotWidget::~MandelbrotWidget()
{
}

void MandelbrotWidget::initializeGL()
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

    // build mandelbrot program
    program_ = compute::program::create_with_source(source, context_);
    program_.build();
}

void MandelbrotWidget::resizeGL(int width, int height)
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

void MandelbrotWidget::paintGL()
{
    using compute::dim;

    float w = width();
    float h = height();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, w, 0.0, h, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // setup the mandelbrot kernel
    compute::kernel kernel(program_, "mandelbrot");
    kernel.set_arg(0, cl_texture_);

    // acquire the opengl texture so it can be used in opencl
    compute::opengl_enqueue_acquire_gl_objects(1, &cl_texture_.get(), queue_);

    // execute the mandelbrot kernel
    queue_.enqueue_nd_range_kernel(
        kernel, dim(0, 0), dim(width(), height()), dim(1, 1)
    );

    // release the opengl texture so it can be used by opengl
    compute::opengl_enqueue_release_gl_objects(1, &cl_texture_.get(), queue_);

    // ensure opencl is finished before rendering in opengl
    queue_.finish();

    // draw a single quad with the mandelbrot image texture
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gl_texture_);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(0, 0);
    glTexCoord2f(0, 1); glVertex2f(0, h);
    glTexCoord2f(1, 1); glVertex2f(w, h);
    glTexCoord2f(1, 0); glVertex2f(w, 0);
    glEnd();
}

void MandelbrotWidget::keyPressEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Escape) {
        this->close();
    }
}

// the mandelbrot example shows how to create a mandelbrot image in
// OpenCL and render the image as a texture in OpenGL
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MandelbrotWidget widget;
    widget.show();

    return app.exec();
}

#include "mandelbrot.moc"
