//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestComplex
#include <boost/test/unit_test.hpp>

#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/fill.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/types/complex.hpp>
#include <boost/compute/type_traits/type_name.hpp>

#include "context_setup.hpp"

// copies a vector of complex<float>'s on the host to the device
BOOST_AUTO_TEST_CASE(copy_complex_vector)
{
    std::vector<std::complex<float> > host_vector;
    host_vector.push_back(std::complex<float>(1.0f, 2.0f));
    host_vector.push_back(std::complex<float>(-2.0f, 1.0f));
    host_vector.push_back(std::complex<float>(1.0f, -2.0f));
    host_vector.push_back(std::complex<float>(-2.0f, -1.0f));

    boost::compute::vector<std::complex<float> > device_vector(context);
    boost::compute::copy(
        host_vector.begin(),
        host_vector.end(),
        device_vector.begin(),
        queue
    );
    queue.finish();
    BOOST_CHECK_EQUAL(std::complex<float>(device_vector[0]), std::complex<float>(1.0f, 2.0f));
    BOOST_CHECK_EQUAL(std::complex<float>(device_vector[1]), std::complex<float>(-2.0f, 1.0f));
    BOOST_CHECK_EQUAL(std::complex<float>(device_vector[2]), std::complex<float>(1.0f, -2.0f));
    BOOST_CHECK_EQUAL(std::complex<float>(device_vector[3]), std::complex<float>(-2.0f, -1.0f));
}

// fills a vector of complex<float>'s on the device with a constant value
BOOST_AUTO_TEST_CASE(fill_complex_vector)
{
    boost::compute::vector<std::complex<float> > vector(6, context);
    boost::compute::fill(
        vector.begin(),
        vector.end(),
        std::complex<float>(2.0f, 5.0f),
        queue
    );
    queue.finish();
    BOOST_CHECK_EQUAL(std::complex<float>(vector[0]), std::complex<float>(2.0f, 5.0f));
    BOOST_CHECK_EQUAL(std::complex<float>(vector[1]), std::complex<float>(2.0f, 5.0f));
    BOOST_CHECK_EQUAL(std::complex<float>(vector[2]), std::complex<float>(2.0f, 5.0f));
    BOOST_CHECK_EQUAL(std::complex<float>(vector[3]), std::complex<float>(2.0f, 5.0f));
    BOOST_CHECK_EQUAL(std::complex<float>(vector[4]), std::complex<float>(2.0f, 5.0f));
    BOOST_CHECK_EQUAL(std::complex<float>(vector[5]), std::complex<float>(2.0f, 5.0f));
}

// extracts the real and imag components of a vector of complex<float>'s using
// transform with the real() and imag() functions
BOOST_AUTO_TEST_CASE(extract_real_and_imag)
{
    boost::compute::vector<std::complex<float> > vector(context);
    vector.push_back(std::complex<float>(1.0f, 3.0f), queue);
    vector.push_back(std::complex<float>(3.0f, 1.0f), queue);
    vector.push_back(std::complex<float>(5.0f, -1.0f), queue);
    vector.push_back(std::complex<float>(7.0f, -3.0f), queue);
    vector.push_back(std::complex<float>(9.0f, -5.0f), queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(5));

    boost::compute::vector<float> reals(5, context);
    boost::compute::transform(
        vector.begin(),
        vector.end(),
        reals.begin(),
        boost::compute::real<float>(),
        queue
    );
    queue.finish();
    BOOST_CHECK_EQUAL(float(reals[0]), float(1.0f));
    BOOST_CHECK_EQUAL(float(reals[1]), float(3.0f));
    BOOST_CHECK_EQUAL(float(reals[2]), float(5.0f));
    BOOST_CHECK_EQUAL(float(reals[3]), float(7.0f));
    BOOST_CHECK_EQUAL(float(reals[4]), float(9.0f));

    boost::compute::vector<float> imags(5, context);
    boost::compute::transform(
        vector.begin(),
        vector.end(),
        imags.begin(),
        boost::compute::imag<float>(),
        queue
    );
    queue.finish();
    BOOST_CHECK_EQUAL(float(imags[0]), float(3.0f));
    BOOST_CHECK_EQUAL(float(imags[1]), float(1.0f));
    BOOST_CHECK_EQUAL(float(imags[2]), float(-1.0f));
    BOOST_CHECK_EQUAL(float(imags[3]), float(-3.0f));
    BOOST_CHECK_EQUAL(float(imags[4]), float(-5.0f));
}

// compute the complex conjugate of a vector of complex<float>'s
BOOST_AUTO_TEST_CASE(complex_conj)
{
    boost::compute::vector<std::complex<float> > input(context);
    input.push_back(std::complex<float>(1.0f, 3.0f), queue);
    input.push_back(std::complex<float>(3.0f, 1.0f), queue);
    input.push_back(std::complex<float>(5.0f, -1.0f), queue);
    input.push_back(std::complex<float>(7.0f, -3.0f), queue);
    input.push_back(std::complex<float>(9.0f, -5.0f), queue);
    BOOST_CHECK_EQUAL(input.size(), size_t(5));

    boost::compute::vector<std::complex<float> > output(5, context);
    boost::compute::transform(
        input.begin(),
        input.end(),
        output.begin(),
        boost::compute::conj<float>(),
        queue
    );
    queue.finish();
    BOOST_CHECK_EQUAL(std::complex<float>(output[0]), std::complex<float>(1.0f, -3.0f));
    BOOST_CHECK_EQUAL(std::complex<float>(output[1]), std::complex<float>(3.0f, -1.0f));
    BOOST_CHECK_EQUAL(std::complex<float>(output[2]), std::complex<float>(5.0f, 1.0f));
    BOOST_CHECK_EQUAL(std::complex<float>(output[3]), std::complex<float>(7.0f, 3.0f));
    BOOST_CHECK_EQUAL(std::complex<float>(output[4]), std::complex<float>(9.0f, 5.0f));
}

// check type_name() for std::complex
BOOST_AUTO_TEST_CASE(complex_type_name)
{
    BOOST_CHECK(
        std::strcmp(
            boost::compute::type_name<std::complex<float> >(),
            "float2"
        ) == 0
    );
}

BOOST_AUTO_TEST_CASE(transform_multiply)
{
    boost::compute::vector<std::complex<float> > x(context);
    x.push_back(std::complex<float>(1.0f, 2.0f), queue);
    x.push_back(std::complex<float>(-2.0f, 5.0f), queue);

    boost::compute::vector<std::complex<float> > y(context);
    y.push_back(std::complex<float>(3.0f, 4.0f), queue);
    y.push_back(std::complex<float>(2.0f, -1.0f), queue);

    boost::compute::vector<std::complex<float> > z(2, context);

    // z = x * y
    boost::compute::transform(
        x.begin(),
        x.end(),
        y.begin(),
        z.begin(),
        boost::compute::multiplies<std::complex<float> >(),
        queue
    );
    queue.finish();

    BOOST_CHECK_EQUAL(std::complex<float>(z[0]), std::complex<float>(-5.0f, 10.0f));
    BOOST_CHECK_EQUAL(std::complex<float>(z[1]), std::complex<float>(1.0f, 12.0f));
}

BOOST_AUTO_TEST_SUITE_END()
