//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

// See boost/compute/detail/diagnostic.hpp
// GCC
#if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 402
#define BOOST_COMPUTE_GCC_DIAG_STR(s) #s
#define BOOST_COMPUTE_GCC_DIAG_JOINSTR(x,y) BOOST_COMPUTE_GCC_DIAG_STR(x ## y)
# define BOOST_COMPUTE_GCC_DIAG_DO_PRAGMA(x) _Pragma (#x)
# define BOOST_COMPUTE_GCC_DIAG_PRAGMA(x) BOOST_COMPUTE_GCC_DIAG_DO_PRAGMA(GCC diagnostic x)
# if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#  define BOOST_COMPUTE_GCC_DIAG_OFF(x) BOOST_COMPUTE_GCC_DIAG_PRAGMA(push) \
      BOOST_COMPUTE_GCC_DIAG_PRAGMA(ignored BOOST_COMPUTE_GCC_DIAG_JOINSTR(-W,x))
#  define BOOST_COMPUTE_GCC_DIAG_ON(x) BOOST_COMPUTE_GCC_DIAG_PRAGMA(pop)
# else
#  define BOOST_COMPUTE_GCC_DIAG_OFF(x) \
      BOOST_COMPUTE_GCC_DIAG_PRAGMA(ignored BOOST_COMPUTE_GCC_DIAG_JOINSTR(-W,x))
#  define BOOST_COMPUTE_GCC_DIAG_ON(x) \
      BOOST_COMPUTE_GCC_DIAG_PRAGMA(warning BOOST_COMPUTE_GCC_DIAG_JOINSTR(-W,x))
# endif
#else // Ensure these macros do nothing for other compilers.
# define BOOST_COMPUTE_GCC_DIAG_OFF(x)
# define BOOST_COMPUTE_GCC_DIAG_ON(x)
#endif

// Clang
#ifdef __clang__
#  define BOOST_COMPUTE_CLANG_DIAG_STR(s) # s
// stringize s to "no-sign-compare"
#  define BOOST_COMPUTE_CLANG_DIAG_JOINSTR(x,y) BOOST_COMPUTE_CLANG_DIAG_STR(x ## y)
//  join -W with no-unused-variable to "-Wno-sign-compare"
#  define BOOST_COMPUTE_CLANG_DIAG_DO_PRAGMA(x) _Pragma (#x)
// _Pragma is unary operator  #pragma ("")
#  define BOOST_COMPUTE_CLANG_DIAG_PRAGMA(x) \
      BOOST_COMPUTE_CLANG_DIAG_DO_PRAGMA(clang diagnostic x)
#  define BOOST_COMPUTE_CLANG_DIAG_OFF(x) BOOST_COMPUTE_CLANG_DIAG_PRAGMA(push) \
      BOOST_COMPUTE_CLANG_DIAG_PRAGMA(ignored BOOST_COMPUTE_CLANG_DIAG_JOINSTR(-W,x))
// For example: #pragma clang diagnostic ignored "-Wno-sign-compare"
#  define BOOST_COMPUTE_CLANG_DIAG_ON(x) BOOST_COMPUTE_CLANG_DIAG_PRAGMA(pop)
// For example: #pragma clang diagnostic warning "-Wno-sign-compare"
#else // Ensure these macros do nothing for other compilers.
#  define BOOST_COMPUTE_CLANG_DIAG_OFF(x)
#  define BOOST_COMPUTE_CLANG_DIAG_ON(x)
#  define BOOST_COMPUTE_CLANG_DIAG_PRAGMA(x)
#endif

// MSVC
#if defined(_MSC_VER)
#  define BOOST_COMPUTE_MSVC_DIAG_DO_PRAGMA(x) __pragma(x)
#  define BOOST_COMPUTE_MSVC_DIAG_PRAGMA(x) \
          BOOST_COMPUTE_MSVC_DIAG_DO_PRAGMA(warning(x))
#  define BOOST_COMPUTE_MSVC_DIAG_OFF(x) BOOST_COMPUTE_MSVC_DIAG_PRAGMA(push) \
          BOOST_COMPUTE_MSVC_DIAG_PRAGMA(disable: x)
#  define BOOST_COMPUTE_MSVC_DIAG_ON(x) BOOST_COMPUTE_MSVC_DIAG_PRAGMA(pop)
#else // Ensure these macros do nothing for other compilers.
#  define BOOST_COMPUTE_MSVC_DIAG_OFF(x)
#  define BOOST_COMPUTE_MSVC_DIAG_ON(x)
#endif

#include <iostream>

// include the proper opencl header for the system
#if defined(__APPLE__)
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

// the opencl_test example displays the opencl platforms and devices found
// on the system using the opencl api directly. if this test fails to compile
// and/or run, there is a problem with the opencl implementation found on the
// system. users should ensure this test runs successfuly before using any of
// the boost.compute apis (which depend on a working opencl implementation).
int main()
{
    // Suppress deprecated declarations warning
    BOOST_COMPUTE_MSVC_DIAG_OFF(4996); // MSVC
    BOOST_COMPUTE_GCC_DIAG_OFF(deprecated-declarations); // GCC
    BOOST_COMPUTE_CLANG_DIAG_OFF(deprecated-declarations); // Clang

    // query number of opencl platforms
    cl_uint num_platforms = 0;
    cl_int ret = clGetPlatformIDs(0, NULL, &num_platforms);
    if(ret != CL_SUCCESS){
        std::cerr << "failed to query platforms: " << ret << std::endl;
        return -1;
    }

    // check that at least one platform was found
    if(num_platforms == 0){
        std::cerr << "found 0 platforms" << std::endl;
        return 0;
    }

    // get platform ids
    cl_platform_id *platforms = new cl_platform_id[num_platforms];
    clGetPlatformIDs(num_platforms, platforms, NULL);

    // iterate through each platform and query its devices
    for(cl_uint i = 0; i < num_platforms; i++){
        cl_platform_id platform = platforms[i];

        // query number of opencl devices
        cl_uint num_devices = 0;
        ret = clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 0, NULL, &num_devices);
        if(ret != CL_SUCCESS){
            std::cerr << "failed to lookup devices for platform " << i << std::endl;
            continue;
        }

        // print number of devices found
        std::cout << "platform " << i << " has " << num_devices << " devices:" << std::endl;

        // get device ids for the platform
        cl_device_id *devices = new cl_device_id[num_devices];
        ret = clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, num_devices, devices, NULL);
        if(ret != CL_SUCCESS){
            std::cerr << "failed to query platform devices" << std::endl;
            delete[] devices;
            continue;
        }

        // iterate through each device on the platform and print its name
        for(cl_uint j = 0; j < num_devices; j++){
            cl_device_id device = devices[j];

            // get length of the device name string
            size_t name_length = 0;
            ret = clGetDeviceInfo(device, CL_DEVICE_NAME, 0, NULL, &name_length);
            if(ret != CL_SUCCESS){
                std::cerr << "failed to query device name length for device " << j << std::endl;
                continue;
            }

            // get the device name string
            char *name = new char[name_length];
            ret = clGetDeviceInfo(device, CL_DEVICE_NAME, name_length, name, NULL);
            if(ret != CL_SUCCESS){
                std::cerr << "failed to query device name string for device " << j << std::endl;
                delete[] name;
                continue;
            }

            // print out the device name
            std::cout << "  device: " << name << std::endl;

            delete[] name;
        }
        delete[] devices;
    }
    delete[] platforms;

    BOOST_COMPUTE_CLANG_DIAG_ON(deprecated-declarations); // Clang
    BOOST_COMPUTE_GCC_DIAG_ON(deprecated-declarations); // GCC
    BOOST_COMPUTE_MSVC_DIAG_OFF(4996); // MSVC

    return 0;
}
