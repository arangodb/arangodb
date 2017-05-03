//---------------------------------------------------------------------------//
// Copyright (c) 2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <iostream>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/copy_n.hpp>
#include <boost/compute/algorithm/find_if.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/zip_iterator.hpp>

namespace compute = boost::compute;

// this example shows how to use the find_if() algorithm to detect the
// point at which two vectors of prices (such as stock prices) cross.
int main()
{
    // get default device and setup context
    compute::device gpu = compute::system::default_device();
    compute::context context(gpu);
    compute::command_queue queue(context, gpu);

    // prices #1 (from 10.0 to 11.0)
    std::vector<float> prices1;
    for(float i = 10.0; i <= 11.0; i += 0.1){
        prices1.push_back(i);
    }

    // prices #2 (from 11.0 to 10.0)
    std::vector<float> prices2;
    for(float i = 11.0; i >= 10.0; i -= 0.1){
        prices2.push_back(i);
    }

    // create gpu vectors
    compute::vector<float> gpu_prices1(prices1.size(), context);
    compute::vector<float> gpu_prices2(prices2.size(), context);

    // copy prices to gpu
    compute::copy(prices1.begin(), prices1.end(), gpu_prices1.begin(), queue);
    compute::copy(prices2.begin(), prices2.end(), gpu_prices2.begin(), queue);

    // function returning true if the second price is less than the first price
    BOOST_COMPUTE_FUNCTION(bool, check_price_cross, (boost::tuple<float, float> prices),
    {
        // first price
        const float first = boost_tuple_get(prices, 0);

        // second price
        const float second = boost_tuple_get(prices, 1);

        // return true if second price is less than first
        return second < first;
    });

    // find cross point (should be 10.5)
    compute::vector<float>::iterator iter = boost::get<0>(
        compute::find_if(
            compute::make_zip_iterator(
                boost::make_tuple(gpu_prices1.begin(), gpu_prices2.begin())
            ),
            compute::make_zip_iterator(
                boost::make_tuple(gpu_prices1.end(), gpu_prices2.end())
            ),
            check_price_cross,
            queue
        ).get_iterator_tuple()
    );

    // print out result
    int index = std::distance(gpu_prices1.begin(), iter);
    std::cout << "price cross at index: " << index << std::endl;

    float value;
    compute::copy_n(iter, 1, &value, queue);
    std::cout << "value: " << value << std::endl;

    return 0;
}
