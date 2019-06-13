//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_CL_HPP
#define BOOST_COMPUTE_CL_HPP

#if defined(BOOST_COMPUTE_MAX_CL_VERSION)
#  if !defined(CL_USE_DEPRECATED_OPENCL_2_1_APIS) && BOOST_COMPUTE_MAX_CL_VERSION < 202
#    define CL_USE_DEPRECATED_OPENCL_2_1_APIS
#  endif
#  if !defined(CL_USE_DEPRECATED_OPENCL_2_0_APIS) && BOOST_COMPUTE_MAX_CL_VERSION < 201
#    define CL_USE_DEPRECATED_OPENCL_2_0_APIS
#  endif
#  if !defined(CL_USE_DEPRECATED_OPENCL_1_2_APIS) && BOOST_COMPUTE_MAX_CL_VERSION < 200
#    define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#  endif
#  if !defined(CL_USE_DEPRECATED_OPENCL_1_1_APIS) && BOOST_COMPUTE_MAX_CL_VERSION < 102
#    define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#  endif
#  if !defined(CL_USE_DEPRECATED_OPENCL_1_0_APIS) && BOOST_COMPUTE_MAX_CL_VERSION < 101
#    define CL_USE_DEPRECATED_OPENCL_1_0_APIS
#  endif
#endif

#if defined(__APPLE__)
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

// select what OpenCL core API versions to use
#if defined(CL_VERSION_1_0)
#  define BOOST_COMPUTE_CL_VERSION_1_0
#endif
#if defined(CL_VERSION_1_1)
#  if !defined(BOOST_COMPUTE_MAX_CL_VERSION) || BOOST_COMPUTE_MAX_CL_VERSION >= 101
#    define BOOST_COMPUTE_CL_VERSION_1_1
#  endif
#endif
#if defined(CL_VERSION_1_2)
#  if !defined(BOOST_COMPUTE_MAX_CL_VERSION) || BOOST_COMPUTE_MAX_CL_VERSION >= 102
#    define BOOST_COMPUTE_CL_VERSION_1_2
#  endif
#endif
#if defined(CL_VERSION_2_0)
#  if !defined(BOOST_COMPUTE_MAX_CL_VERSION) || BOOST_COMPUTE_MAX_CL_VERSION >= 200
#    define BOOST_COMPUTE_CL_VERSION_2_0
#  endif
#endif
#if defined(CL_VERSION_2_1)
#  if !defined(BOOST_COMPUTE_MAX_CL_VERSION) || BOOST_COMPUTE_MAX_CL_VERSION >= 201
#    define BOOST_COMPUTE_CL_VERSION_2_1
#  endif
#endif
#if defined(CL_VERSION_2_2)
#  if !defined(BOOST_COMPUTE_MAX_CL_VERSION) || BOOST_COMPUTE_MAX_CL_VERSION >= 202
#    define BOOST_COMPUTE_CL_VERSION_2_2
#  endif
#endif

#endif // BOOST_COMPUTE_CL_HPP
