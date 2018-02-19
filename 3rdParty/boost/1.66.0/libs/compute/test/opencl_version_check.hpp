//---------------------------------------------------------------------------//
// Copyright (c) 2014 Denis Demidov
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_TEST_OPENCL_VERSION_CHECK_HPP
#define BOOST_COMPUTE_TEST_OPENCL_VERSION_CHECK_HPP

#define REQUIRES_OPENCL_VERSION(major, minor) \
    if (!device.check_version(major, minor)) return

#define REQUIRES_OPENCL_PLATFORM_VERSION(major, minor) \
    if (!device.platform().check_version(major, minor)) return

#endif
