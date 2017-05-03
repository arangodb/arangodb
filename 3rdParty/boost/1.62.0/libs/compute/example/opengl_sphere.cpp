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

#include <GL/gl.h>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkgl.h>
#include <vtkInteractorStyleSwitch.h>
#include <vtkMapper.h>
#include <vtkObjectFactory.h>
#include <vtkOpenGLExtensionManager.h>
#include <vtkOpenGLRenderWindow.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/interop/opengl.hpp>
#include <boost/compute/interop/vtk.hpp>
#include <boost/compute/utility/dim.hpp>
#include <boost/compute/utility/source.hpp>

namespace compute = boost::compute;

// tesselates a sphere with radius, phi_slices, and theta_slices. returns
// a shared opencl/opengl buffer containing the vertex data.
compute::opengl_buffer tesselate_sphere(float radius,
                                        size_t phi_slices,
                                        size_t theta_slices,
                                        compute::command_queue &queue)
{
    using compute::dim;

    const compute::context &context = queue.get_context();

    const size_t vertex_count = phi_slices * theta_slices;

    // create opengl buffer
    GLuint vbo;
    vtkgl::GenBuffersARB(1, &vbo);
    vtkgl::BindBufferARB(vtkgl::ARRAY_BUFFER, vbo);
    vtkgl::BufferDataARB(vtkgl::ARRAY_BUFFER,
                         sizeof(float) * 4 * vertex_count,
                         NULL,
                         vtkgl::STREAM_DRAW);
    vtkgl::BindBufferARB(vtkgl::ARRAY_BUFFER, 0);

    // create shared opengl/opencl buffer
    compute::opengl_buffer vertex_buffer(context, vbo);

    // tesselate_sphere kernel source
    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void tesselate_sphere(float radius,
                                       uint phi_slices,
                                       uint theta_slices,
                                       __global float4 *vertex_buffer)
        {
            const uint phi_i = get_global_id(0);
            const uint theta_i = get_global_id(1);

            const float phi = phi_i * 2.f * M_PI_F / phi_slices;
            const float theta = theta_i * 2.f * M_PI_F / theta_slices;

            float4 v;
            v.x = radius * cos(theta) * cos(phi);
            v.y = radius * cos(theta) * sin(phi);
            v.z = radius * sin(theta);
            v.w = 1.f;

            vertex_buffer[phi_i*phi_slices+theta_i] = v;
        }
    );

    // build tesselate_sphere program
    compute::program program =
        compute::program::create_with_source(source, context);
    program.build();

    // setup tesselate_sphere kernel
    compute::kernel kernel(program, "tesselate_sphere");
    kernel.set_arg<compute::float_>(0, radius);
    kernel.set_arg<compute::uint_>(1, phi_slices);
    kernel.set_arg<compute::uint_>(2, theta_slices);
    kernel.set_arg(3, vertex_buffer);

    // acqurire buffer so that it is accessible to OpenCL
    compute::opengl_enqueue_acquire_buffer(vertex_buffer, queue);

    // execute tesselate_sphere kernel
    queue.enqueue_nd_range_kernel(
        kernel, dim(0, 0), dim(phi_slices, theta_slices), dim(1, 1)
    );

    // release buffer so that it is accessible to OpenGL
    compute::opengl_enqueue_release_buffer(vertex_buffer, queue);

    return vertex_buffer;
}

// simple vtkMapper subclass to render the tesselated sphere on the gpu.
class gpu_sphere_mapper : public vtkMapper
{
public:
    vtkTypeMacro(gpu_sphere_mapper, vtkMapper)

    static gpu_sphere_mapper* New()
    {
        return new gpu_sphere_mapper;
    }

    void Render(vtkRenderer *renderer, vtkActor *actor)
    {
        if(!m_initialized){
            Initialize(renderer, actor);
            m_initialized = true;
        }

        if(!m_tesselated){
            m_vertex_count = m_phi_slices * m_theta_slices;

            // tesselate sphere
            m_vertex_buffer = tesselate_sphere(
                m_radius, m_phi_slices, m_theta_slices, m_command_queue
            );

            // ensure tesselation is finished (seems to be required on AMD)
            m_command_queue.finish();

            // set tesselated flag to true
            m_tesselated = true;
        }

        // draw sphere
        glEnableClientState(GL_VERTEX_ARRAY);
        vtkgl::BindBufferARB(vtkgl::ARRAY_BUFFER, m_vertex_buffer.get_opengl_object());
        glVertexPointer(4, GL_FLOAT, sizeof(float)*4, 0);
        glDrawArrays(GL_POINTS, 0, m_vertex_count);
    }

    void Initialize(vtkRenderer *renderer, vtkActor *actor)
    {
        // initialize opengl extensions
        vtkOpenGLExtensionManager *extensions =
            static_cast<vtkOpenGLRenderWindow *>(renderer->GetRenderWindow())
                ->GetExtensionManager();
        extensions->LoadExtension("GL_ARB_vertex_buffer_object");

        // initialize opencl/opengl shared context
        m_context = compute::opengl_create_shared_context();
        compute::device device = m_context.get_device();
        std::cout << "device: " << device.name() << std::endl;

        // create command queue for the gpu device
        m_command_queue = compute::command_queue(m_context, device);
    }

    double* GetBounds()
    {
        static double bounds[6];
        bounds[0] = -m_radius; bounds[1] = m_radius;
        bounds[2] = -m_radius; bounds[3] = m_radius;
        bounds[4] = -m_radius; bounds[5] = m_radius;
        return bounds;
    }

protected:
    gpu_sphere_mapper()
    {
        m_radius = 5.0f;
        m_phi_slices = 100;
        m_theta_slices = 100;
        m_initialized = false;
        m_tesselated = false;
    }

private:
    float m_radius;
    int m_phi_slices;
    int m_theta_slices;
    int m_vertex_count;
    bool m_initialized;
    bool m_tesselated;
    compute::context m_context;
    compute::command_queue m_command_queue;
    compute::opengl_buffer m_vertex_buffer;
};

int main(int argc, char *argv[])
{
    // create gpu sphere mapper
    vtkSmartPointer<gpu_sphere_mapper> mapper =
        vtkSmartPointer<gpu_sphere_mapper>::New();

    // create actor for gpu sphere mapper
    vtkSmartPointer<vtkActor> actor =
        vtkSmartPointer<vtkActor>::New();
    actor->GetProperty()->LightingOff();
    actor->GetProperty()->SetInterpolationToFlat();
    actor->SetMapper(mapper);

    // create render window
    vtkSmartPointer<vtkRenderer> renderer =
        vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(.1, .2, .31);
    vtkSmartPointer<vtkRenderWindow> renderWindow =
        vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->SetSize(800, 600);
    renderWindow->AddRenderer(renderer);
    vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor =
        vtkSmartPointer<vtkRenderWindowInteractor>::New();
    vtkInteractorStyleSwitch *interactorStyle =
        vtkInteractorStyleSwitch::SafeDownCast(
            renderWindowInteractor->GetInteractorStyle()
        );
    interactorStyle->SetCurrentStyleToTrackballCamera();
    renderWindowInteractor->SetRenderWindow(renderWindow);
    renderer->AddActor(actor);

    // render
    renderer->ResetCamera();
    vtkCamera *camera = renderer->GetActiveCamera();
    camera->Elevation(-90.0);
    renderWindowInteractor->Initialize();
    renderWindow->Render();
    renderWindowInteractor->Start();

    return 0;
}
