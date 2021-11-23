//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestInteropVTK
#include <boost/test/unit_test.hpp>

#include <vtkFloatArray.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkSmartPointer.h>
#include <vtkUnsignedCharArray.h>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/sort.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/detail/is_contiguous_iterator.hpp>
#include <boost/compute/interop/vtk.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(bounds)
{
    using compute::float4_;

    // create vtk points
    vtkNew<vtkPoints> points;
    points->InsertNextPoint(0.0, 0.0, 0.0);
    points->InsertNextPoint(1.0, 2.0, 1.0);
    points->InsertNextPoint(-1.0, -3.0, -1.0);
    points->InsertNextPoint(0.5, 2.5, 1.5);

    // copy points to vector on gpu
    compute::vector<float4_> vector(points->GetNumberOfPoints(), context);
    compute::vtk_copy_points_to_buffer(points.GetPointer(), vector.begin(), queue);

    // compute bounds
    double bounds[6];
    compute::vtk_compute_bounds(vector.begin(), vector.end(), bounds, queue);

    // check bounds
    BOOST_CHECK_CLOSE(bounds[0], -1.0, 1e-8);
    BOOST_CHECK_CLOSE(bounds[1],  1.0, 1e-8);
    BOOST_CHECK_CLOSE(bounds[2], -3.0, 1e-8);
    BOOST_CHECK_CLOSE(bounds[3],  2.5, 1e-8);
    BOOST_CHECK_CLOSE(bounds[4], -1.0, 1e-8);
    BOOST_CHECK_CLOSE(bounds[5],  1.5, 1e-8);
}

BOOST_AUTO_TEST_CASE(copy_uchar_array)
{
    // create vtk uchar vector containing 3 RGBA colors
    vtkNew<vtkUnsignedCharArray> array;
    array->SetNumberOfComponents(4);

    unsigned char red[4] = { 255, 0, 0, 255 };
    array->InsertNextTupleValue(red);
    unsigned char green[4] = { 0, 255, 0, 255 };
    array->InsertNextTupleValue(green);
    unsigned char blue[4] = { 0, 0, 255, 255 };
    array->InsertNextTupleValue(blue);

    // create vector<uchar4> on device and copy values from vtk array
    compute::vector<compute::uchar4_> vector(3, context);
    compute::vtk_copy_data_array_to_buffer(
        array.GetPointer(),
        compute::make_buffer_iterator<compute::uchar_>(vector.get_buffer(), 0),
        queue
    );

    // check values
    std::vector<compute::uchar4_> host_vector(3);
    compute::copy(
        vector.begin(), vector.end(), host_vector.begin(), queue
    );
    BOOST_CHECK(host_vector[0] == compute::uchar4_(255, 0, 0, 255));
    BOOST_CHECK(host_vector[1] == compute::uchar4_(0, 255, 0, 255));
    BOOST_CHECK(host_vector[2] == compute::uchar4_(0, 0, 255, 255));
}

BOOST_AUTO_TEST_CASE(sort_float_array)
{
    // create vtk float array
    vtkNew<vtkFloatArray> array;
    array->InsertNextValue(2.5f);
    array->InsertNextValue(1.0f);
    array->InsertNextValue(6.5f);
    array->InsertNextValue(4.0f);

    // create vector on device and copy values from vtk array
    compute::vector<float> vector(4, context);
    compute::vtk_copy_data_array_to_buffer(array.GetPointer(), vector.begin(), queue);

    // sort values on the gpu
    compute::sort(vector.begin(), vector.end(), queue);
    CHECK_RANGE_EQUAL(float, 4, vector, (1.0f, 2.5f, 4.0f, 6.5f));

    // copy sorted values back to the vtk array
    compute::vtk_copy_buffer_to_data_array(
        vector.begin(), vector.end(), array.GetPointer(), queue
    );
    BOOST_CHECK_EQUAL(array->GetValue(0), 1.0f);
    BOOST_CHECK_EQUAL(array->GetValue(1), 2.5f);
    BOOST_CHECK_EQUAL(array->GetValue(2), 4.0f);
    BOOST_CHECK_EQUAL(array->GetValue(3), 6.5f);
}

BOOST_AUTO_TEST_SUITE_END()
