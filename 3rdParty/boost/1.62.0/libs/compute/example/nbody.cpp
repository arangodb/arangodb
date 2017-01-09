//---------------------------------------------------------------------------//
// Copyright (c) 2014 Fabian Köhler <fabian2804@googlemail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <iostream>

#define GL_GLEXT_PROTOTYPES
#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include <QtGlobal>
#if QT_VERSION >= 0x050000
#include <QtWidgets>
#else
#include <QtGui>
#endif
#include <QtOpenGL>
#include <QTimer>

#include <boost/program_options.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>

#ifndef Q_MOC_RUN
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/interop/opengl.hpp>
#include <boost/compute/utility/source.hpp>
#endif // Q_MOC_RUN

namespace compute = boost::compute;
namespace po = boost::program_options;

using compute::uint_;
using compute::float4_;

const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
    __kernel void updateVelocity(__global const float4* position, __global float4* velocity, float dt, uint N)
    {
        uint gid = get_global_id(0);

        float4 r = { 0.0f, 0.0f, 0.0f, 0.0f };
        float f = 0.0f;
        for(uint i = 0; i != gid; i++) {
            if(i != gid) {
                r = position[i]-position[gid];
                f = length(r)+0.001f;
                f *= f*f;
                f = dt/f;
                velocity[gid] += f*r;
            }
        }
    }
    __kernel void updatePosition(__global float4* position, __global const float4* velocity, float dt)
    {
        uint gid = get_global_id(0);

        position[gid].xyz += dt*velocity[gid].xyz;
    }
);

class NBodyWidget : public QGLWidget
{
    Q_OBJECT

public:
    NBodyWidget(std::size_t particles, float dt, QWidget* parent = 0);
    ~NBodyWidget();

    void initializeGL();
    void resizeGL(int width, int height);
    void paintGL();
    void updateParticles();
    void keyPressEvent(QKeyEvent* event);

private:
    QTimer* timer;

    compute::context m_context;
    compute::command_queue m_queue;
    compute::program m_program;
    compute::opengl_buffer m_position;
    compute::vector<float4_>* m_velocity;
    compute::kernel m_velocity_kernel;
    compute::kernel m_position_kernel;

    bool m_initial_draw;

    const uint_ m_particles;
    const float m_dt;
};

NBodyWidget::NBodyWidget(std::size_t particles, float dt, QWidget* parent)
    : QGLWidget(parent), m_initial_draw(true), m_particles(particles), m_dt(dt)
{
    // create a timer to redraw as fast as possible
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateGL()));
    timer->start(1);
}

NBodyWidget::~NBodyWidget()
{
    delete m_velocity;

    // delete the opengl buffer
    GLuint vbo = m_position.get_opengl_object();
    glDeleteBuffers(1, &vbo);
}

void NBodyWidget::initializeGL()
{
    // create context, command queue and program
    m_context = compute::opengl_create_shared_context();
    m_queue = compute::command_queue(m_context, m_context.get_device());
    m_program = compute::program::create_with_source(source, m_context);
    m_program.build();

    // prepare random particle positions that will be transferred to the vbo
    float4_* temp = new float4_[m_particles];
    boost::random::uniform_real_distribution<float> dist(-0.5f, 0.5f);
    boost::random::mt19937_64 gen;
    for(size_t i = 0; i < m_particles; i++) {
        temp[i][0] = dist(gen);
        temp[i][1] = dist(gen);
        temp[i][2] = dist(gen);
        temp[i][3] = 1.0f;
    }

    // create an OpenGL vbo
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, m_particles*sizeof(float4_), temp, GL_DYNAMIC_DRAW);

    // create a OpenCL buffer from the vbo
    m_position = compute::opengl_buffer(m_context, vbo);
    delete[] temp;

    // create buffer for velocities
    m_velocity = new compute::vector<float4_>(m_particles, m_context);
    compute::fill(m_velocity->begin(), m_velocity->end(), float4_(0.0f, 0.0f, 0.0f, 0.0f), m_queue);

    // create compute kernels
    m_velocity_kernel = m_program.create_kernel("updateVelocity");
    m_velocity_kernel.set_arg(0, m_position);
    m_velocity_kernel.set_arg(1, m_velocity->get_buffer());
    m_velocity_kernel.set_arg(2, m_dt);
    m_velocity_kernel.set_arg(3, m_particles);
    m_position_kernel = m_program.create_kernel("updatePosition");
    m_position_kernel.set_arg(0, m_position);
    m_position_kernel.set_arg(1, m_velocity->get_buffer());
    m_position_kernel.set_arg(2, m_dt);
}
void NBodyWidget::resizeGL(int width, int height)
{
    // update viewport
    glViewport(0, 0, width, height);
}
void NBodyWidget::paintGL()
{
    // clear buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // check if this is the first draw
    if(m_initial_draw) {
        // do not update particles
        m_initial_draw = false;
    } else {
        // update particles
        updateParticles();
    }

    // draw
    glVertexPointer(4, GL_FLOAT, 0, 0);
    glEnableClientState(GL_VERTEX_ARRAY);
    glDrawArrays(GL_POINTS, 0, m_particles);
    glFinish();
}
void NBodyWidget::updateParticles()
{
    // enqueue kernels to update particles and make sure that the command queue is finished
    compute::opengl_enqueue_acquire_buffer(m_position, m_queue);
    m_queue.enqueue_1d_range_kernel(m_velocity_kernel, 0, m_particles, 0).wait();
    m_queue.enqueue_1d_range_kernel(m_position_kernel, 0, m_particles, 0).wait();
    m_queue.finish();
    compute::opengl_enqueue_release_buffer(m_position, m_queue);
}
void NBodyWidget::keyPressEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Escape) {
        this->close();
    }
}

int main(int argc, char** argv)
{
    // parse command line arguments
    po::options_description options("options");
    options.add_options()
        ("help", "show usage")
        ("particles", po::value<uint_>()->default_value(1000), "number of particles")
        ("dt", po::value<float>()->default_value(0.00001f), "width of each integration step");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, options), vm);
    po::notify(vm);

    if(vm.count("help") > 0) {
        std::cout << options << std::endl;
        return 0;
    }

    const uint_ particles = vm["particles"].as<uint_>();
    const float dt = vm["dt"].as<float>();

    QApplication app(argc, argv);
    NBodyWidget nbody(particles, dt);

    nbody.show();

    return app.exec();
}

#include "nbody.moc"
