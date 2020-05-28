//---------------------------------------------------------------------------//
// Copyright (c) 2015 Jakub Pola <jakub.pola@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestScatterIf
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/scatter_if.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/constant_buffer_iterator.hpp>
#include <boost/compute/iterator/counting_iterator.hpp>
#include <boost/compute/functional.hpp>
#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(scatter_if_int)
{
    int input_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    bc::vector<int> input(input_data, input_data + 10, queue);

    int map_data[] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

    bc::vector<int> map(map_data, map_data + 10, queue);

    int stencil_data[] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1};

    bc::vector<bc::uint_> stencil(stencil_data, stencil_data + 10, queue);

    bc::vector<int> output(input.size(), -1, queue);

    bc::scatter_if(input.begin(), input.end(),
                   map.begin(), stencil.begin(),
                   output.begin(),
                   queue);

    CHECK_RANGE_EQUAL(int, 10, output, (9, -1, 7, -1, 5, -1, 3, -1, 1, -1) );
}

BOOST_AUTO_TEST_CASE(scatter_if_constant_indices)
{
    int input_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    bc::vector<int> input(input_data, input_data + 10, queue);

    int map_data[] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    bc::buffer map_buffer(context,
                          10 * sizeof(int),
                          bc::buffer::read_only | bc::buffer::use_host_ptr,
                          map_data);

    int stencil_data[] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
    bc::buffer stencil_buffer(context,
                              10 * sizeof(bc::uint_),
                              bc::buffer::read_only | bc::buffer::use_host_ptr,
                              stencil_data);

    bc::vector<int> output(input.size(), -1, queue);

    bc::scatter_if(input.begin(),
                   input.end(),
                   bc::make_constant_buffer_iterator<int>(map_buffer, 0),
                   bc::make_constant_buffer_iterator<int>(stencil_buffer, 0),
                   output.begin(),
                   queue);

    CHECK_RANGE_EQUAL(int, 10, output, (9, -1, 7, -1, 5, -1, 3, -1, 1, -1) );
}


BOOST_AUTO_TEST_CASE(scatter_if_function)
{
    int input_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    bc::vector<int> input(input_data, input_data + 10, queue);

    int map_data[] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    bc::vector<int> map(map_data, map_data + 10, queue);

    int stencil_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    bc::vector<bc::uint_> stencil(stencil_data, stencil_data + 10, queue);

    bc::vector<int> output(input.size(), -1, queue);

    BOOST_COMPUTE_FUNCTION(int, gt_than_5, (int x),
    {
        if (x > 5)
            return true;
        else
            return false;
    });

    bc::scatter_if(input.begin(),
                   input.end(),
                   map.begin(),
                   stencil.begin(),
                   output.begin(),
                   gt_than_5,
                   queue);

    CHECK_RANGE_EQUAL(int, 10, output, (9, 8, 7, 6, -1, -1, -1, -1, -1, -1) );
}


BOOST_AUTO_TEST_CASE(scatter_if_counting_iterator)
{
    int input_data[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    bc::vector<int> input(input_data, input_data + 10, queue);

    int map_data[] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    bc::vector<int> map(map_data, map_data + 10, queue);

    bc::vector<int> output(input.size(), -1, queue);

    BOOST_COMPUTE_FUNCTION(int, gt_than_5, (int x),
    {
        if (x > 5)
            return true;
        else
            return false;
    });

    bc::scatter_if(input.begin(),
                   input.end(),
                   map.begin(),
                   bc::make_counting_iterator<int>(0),
                   output.begin(),
                   gt_than_5,
                   queue);

    CHECK_RANGE_EQUAL(int, 10, output, (9, 8, 7, 6, -1, -1, -1, -1, -1, -1) );

}

BOOST_AUTO_TEST_SUITE_END()
