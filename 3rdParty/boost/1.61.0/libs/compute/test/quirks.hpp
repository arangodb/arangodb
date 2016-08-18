//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_TEST_QUIRKS_HPP
#define BOOST_COMPUTE_TEST_QUIRKS_HPP

#include <boost/compute/device.hpp>
#include <boost/compute/platform.hpp>
#include <boost/compute/detail/vendor.hpp>

// this file contains functions which check for 'quirks' or buggy
// behavior in OpenCL implementations. this allows us to skip certain
// tests when running on buggy platforms.

// returns true if the device is a POCL device
inline bool is_pocl_device(const boost::compute::device &device)
{
    return device.platform().name() == "Portable Computing Language";
}

// AMD platforms have a bug when using struct assignment. this affects
// algorithms like fill() when used with pairs/tuples.
//
// see: https://community.amd.com/thread/166622
inline bool bug_in_struct_assignment(const boost::compute::device &device)
{
    return boost::compute::detail::is_amd_device(device);
}

// clEnqueueSVMMemcpy() operation does not work on AMD devices. This affects
// copy() algorithm.
//
// see: https://community.amd.com/thread/190585
inline bool bug_in_svmmemcpy(const boost::compute::device &device)
{
    return boost::compute::detail::is_amd_device(device);
}

// returns true if the device supports image samplers.
inline bool supports_image_samplers(const boost::compute::device &device)
{
    // POCL does not yet support image samplers and gives the following
    // error when attempting to create one:
    //
    // pocl error: encountered unimplemented part of the OpenCL specs
    // in clCreateSampler.c:28
    if(is_pocl_device(device)){
        return false;
    }

    return true;
}

// returns true if the device supports clSetMemObjectDestructorCallback
inline bool supports_destructor_callback(const boost::compute::device &device)
{
    // unimplemented in POCL
    return !is_pocl_device(device);
}

// returns true if the device supports clCompileProgram
inline bool supports_compile_program(const boost::compute::device &device)
{
    // unimplemented in POCL
    return !is_pocl_device(device);
}

// returns true if the device supports clLinkProgram
inline bool supports_link_program(const boost::compute::device &device)
{
    // unimplemented in POCL
    return !is_pocl_device(device);
}

#endif // BOOST_COMPUTE_TEST_QUIRKS_HPP
