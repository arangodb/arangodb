//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestDevice
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/compute/device.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/detail/nvidia_compute_capability.hpp>

#include "opencl_version_check.hpp"

BOOST_AUTO_TEST_CASE(null_device)
{
    boost::compute::device null;
    BOOST_CHECK(null.id() == cl_device_id());
    BOOST_CHECK(null.get() == cl_device_id());
}

BOOST_AUTO_TEST_CASE(default_device_doctest)
{
//! [default_gpu]
boost::compute::device gpu = boost::compute::system::default_device();
//! [default_gpu]

    BOOST_CHECK(gpu.id());
}

BOOST_AUTO_TEST_CASE(device_platform)
{
    boost::compute::platform p = boost::compute::system::platforms().at(0);
    BOOST_CHECK(p == p.devices().at(0).platform());
}

BOOST_AUTO_TEST_CASE(get_device_name)
{
    boost::compute::device gpu = boost::compute::system::default_device();
    if(gpu.id()){
        BOOST_CHECK(!gpu.name().empty());
    }
}

BOOST_AUTO_TEST_CASE(equality_operator)
{
    boost::compute::device device1 = boost::compute::system::default_device();
    BOOST_CHECK(device1 == device1);

    boost::compute::device device2 = device1;
    BOOST_CHECK(device1 == device2);
}

BOOST_AUTO_TEST_CASE(get_max_work_item_sizes)
{
    boost::compute::device device = boost::compute::system::default_device();

    std::vector<size_t> max_work_item_sizes =
        device.get_info<std::vector<size_t> >(CL_DEVICE_MAX_WORK_ITEM_SIZES);
    BOOST_CHECK_GE(max_work_item_sizes.size(), size_t(3));
    BOOST_CHECK_GE(max_work_item_sizes[0], size_t(1));
    BOOST_CHECK_GE(max_work_item_sizes[1], size_t(1));
    BOOST_CHECK_GE(max_work_item_sizes[2], size_t(1));
}

#ifdef CL_VERSION_1_2

// returns true if the device supports the partitioning type
bool supports_partition_type(const boost::compute::device &device,
                             cl_device_partition_property type)
{
    const std::vector<cl_device_partition_property> properties =
        device.get_info<std::vector<cl_device_partition_property> >(
            CL_DEVICE_PARTITION_PROPERTIES
        );

    return std::find(properties.begin(),
                     properties.end(),
                     type) != properties.end();
}

BOOST_AUTO_TEST_CASE(partition_device_equally)
{
    // get default device and ensure it has at least two compute units
    boost::compute::device device = boost::compute::system::default_device();

    REQUIRES_OPENCL_VERSION(1,2);

    if(device.compute_units() < 2){
        std::cout << "skipping test: "
                  << "device does not have enough compute units"
                  << std::endl;
        return;
    }

    // check that the device supports partitioning equally
    if(!supports_partition_type(device, CL_DEVICE_PARTITION_EQUALLY)){
        std::cout << "skipping test: "
                  << "device does not support CL_DEVICE_PARTITION_EQUALLY"
                  << std::endl;
        return;
    }

    // ensure device is not a sub-device
    BOOST_CHECK(device.is_subdevice() == false);

    // partition default device into sub-devices with two compute units each
    std::vector<boost::compute::device>
        sub_devices = device.partition_equally(2);
    BOOST_CHECK_EQUAL(sub_devices.size(), size_t(device.compute_units() / 2));

    // verify each of the sub-devices
    for(size_t i = 0; i < sub_devices.size(); i++){
        const boost::compute::device &sub_device = sub_devices[i];

        // ensure parent device id is correct
        cl_device_id parent_id =
            sub_device.get_info<cl_device_id>(CL_DEVICE_PARENT_DEVICE);
        BOOST_CHECK(parent_id == device.id());

        // ensure device is a sub-device
        BOOST_CHECK(sub_device.is_subdevice() == true);

        // check number of compute units
        BOOST_CHECK_EQUAL(sub_device.compute_units(), size_t(2));
    }
}

// used to sort devices by number of compute units
bool compare_compute_units(const boost::compute::device &a,
                           const boost::compute::device &b)
{
    return a.compute_units() < b.compute_units();
}

BOOST_AUTO_TEST_CASE(partition_by_counts)
{
    // get default device and ensure it has at least four compute units
    boost::compute::device device = boost::compute::system::default_device();

    REQUIRES_OPENCL_VERSION(1,2);

    if(device.compute_units() < 4){
        std::cout << "skipping test: "
                  << "device does not have enough compute units"
                  << std::endl;
        return;
    }

    // check that the device supports partitioning by counts
    if(!supports_partition_type(device, CL_DEVICE_PARTITION_BY_COUNTS)){
        std::cout << "skipping test: "
                  << "device does not support CL_DEVICE_PARTITION_BY_COUNTS"
                  << std::endl;
        return;
    }

    // ensure device is not a sub-device
    BOOST_CHECK(device.is_subdevice() == false);

    // create vector of sub-device compute unit counts
    std::vector<size_t> counts;
    counts.push_back(2);
    counts.push_back(1);
    counts.push_back(1);

    // partition default device into sub-devices according to counts
    std::vector<boost::compute::device>
        sub_devices = device.partition_by_counts(counts);
    BOOST_CHECK_EQUAL(sub_devices.size(), size_t(3));

    // sort sub-devices by number of compute units (see issue #185)
    std::sort(sub_devices.begin(), sub_devices.end(), compare_compute_units);

    // verify each of the sub-devices
    BOOST_CHECK_EQUAL(sub_devices[0].compute_units(), size_t(1));
    BOOST_CHECK_EQUAL(sub_devices[1].compute_units(), size_t(1));
    BOOST_CHECK_EQUAL(sub_devices[2].compute_units(), size_t(2));
}

BOOST_AUTO_TEST_CASE(partition_by_affinity_domain)
{
    // get default device and ensure it has at least two compute units
    boost::compute::device device = boost::compute::system::default_device();

    REQUIRES_OPENCL_VERSION(1,2);

    if(device.compute_units() < 2){
        std::cout << "skipping test: "
                  << "device does not have enough compute units"
                  << std::endl;
        return;
    }

    // check that the device supports splitting by affinity domains
    if(!supports_partition_type(device, CL_DEVICE_AFFINITY_DOMAIN_NEXT_PARTITIONABLE)){
        std::cout << "skipping test: "
                  << "device does not support partitioning by affinity domain"
                  << std::endl;
        return;
    }

    // ensure device is not a sub-device
    BOOST_CHECK(device.is_subdevice() == false);

    std::vector<boost::compute::device> sub_devices =
        device.partition_by_affinity_domain(
            CL_DEVICE_AFFINITY_DOMAIN_NEXT_PARTITIONABLE);
    BOOST_CHECK(sub_devices.size() > 0);
    BOOST_CHECK(sub_devices[0].is_subdevice() == true);
}
#endif // CL_VERSION_1_2

BOOST_AUTO_TEST_CASE(nvidia_compute_capability)
{
    boost::compute::device device = boost::compute::system::default_device();
    int major, minor;
    boost::compute::detail::get_nvidia_compute_capability(device, major, minor);
    boost::compute::detail::check_nvidia_compute_capability(device, 3, 0);
}

BOOST_AUTO_TEST_CASE(get_info_specializations)
{
    boost::compute::device device = boost::compute::system::default_device();

    std::cout << device.get_info<CL_DEVICE_NAME>() << std::endl;
}
