//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestUserDefinedTypes
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/compute/function.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/sort.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/types/struct.hpp>

namespace compute = boost::compute;

// user-defined data type containing two int's and a float
struct UDD
{
    int a;
    int b;
    float c;
};

// make UDD available to OpenCL
BOOST_COMPUTE_ADAPT_STRUCT(UDD, UDD, (a, b, c))

// comparison operator for UDD
bool operator==(const UDD &lhs, const UDD &rhs)
{
    return lhs.a == rhs.a && lhs.b == rhs.b && lhs.c == rhs.c;
}

// output stream operator for UDD
std::ostream& operator<<(std::ostream &stream, const UDD &x)
{
    return stream << "(" << x.a << ", " << x.b << ", " << x.c << ")";
}

// function to generate a random UDD on the host
UDD rand_UDD()
{
    UDD udd;
    udd.a = rand() % 100;
    udd.b = rand() % 100;
    udd.c = (float)(rand() % 100) / 1.3f;

    return udd;
}

// function to compare two UDD's on the host by their first component
bool compare_UDD_host(const UDD &lhs, const UDD &rhs)
{
    return lhs.a < rhs.a;
}

// function to compate two UDD's on the device by their first component
BOOST_COMPUTE_FUNCTION(bool, compare_UDD_device, (UDD lhs, UDD rhs),
{
    return lhs.a < rhs.a;
});

#include "check_macros.hpp"
#include "context_setup.hpp"

// see: issue #11 (https://github.com/boostorg/compute/issues/11)
BOOST_AUTO_TEST_CASE(issue_11)
{
    if(device.vendor() == "NVIDIA" && device.platform().name() == "Apple"){
        // FIXME: this test currently segfaults on NVIDIA GPUs on Apple
        std::cerr << "skipping issue test on NVIDIA GPU on Apple platform" << std::endl;
        return;
    }

    // create vector of random values on the host
    std::vector<UDD> host_vector(10);
    std::generate(host_vector.begin(), host_vector.end(), rand_UDD);

    // transfer the values to the device
    compute::vector<UDD> device_vector(host_vector.size(), context);
    compute::copy(
        host_vector.begin(), host_vector.end(), device_vector.begin(), queue
    );

    // sort values on the device
    compute::sort(
        device_vector.begin(),
        device_vector.end(),
        compare_UDD_device,
        queue
    );

    // sort values on the host
    std::sort(
        host_vector.begin(),
        host_vector.end(),
        compare_UDD_host
    );

    // copy sorted device values back to the host
    std::vector<UDD> tmp(10);
    compute::copy(
        device_vector.begin(),
        device_vector.end(),
        tmp.begin(),
        queue
    );

    // verify sorted values
    for(size_t i = 0; i < host_vector.size(); i++){
        BOOST_CHECK_EQUAL(tmp[i], host_vector[i]);
    }
}

BOOST_AUTO_TEST_SUITE_END()
