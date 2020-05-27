//---------------------------------------------------------------------------//
// Copyright (c) 2014 Benoit Dequidt <benoit.dequidt@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <iostream>
#include <cstdlib>

#include <boost/compute/core.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/inclusive_scan.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/type_traits/type_name.hpp>
#include <boost/compute/utility/source.hpp>

namespace compute = boost::compute;

/// warning precision is not precise due
/// to the float error accumulation when size is large enough
/// for more precision use double
/// or a kahan sum else results can diverge
/// from the CPU implementation
compute::program make_sma_program(const compute::context& context)
{
    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void SMA(__global const float *scannedValues, int size, __global float *output, int wSize)
        {
            const int gid = get_global_id(0);

            float cumValues = 0.f;
            int endIdx = gid + wSize/2;
            int startIdx = gid -1 - wSize/2;

            if(endIdx > size -1)
                endIdx = size -1;

            cumValues += scannedValues[endIdx];
            if(startIdx < 0)
                startIdx = -1;
            else
                cumValues -= scannedValues[startIdx];

            output[gid] =(float)( cumValues / ( float )(endIdx - startIdx));
        }
   );

    // create sma program
    return compute::program::build_with_source(source,context);
}

bool check_results(const std::vector<float>& values, const std::vector<float>& smoothValues, unsigned int wSize)
{
    int size = values.size();
    if(size != (int)smoothValues.size()) return false;

    int semiWidth = wSize/2;

    bool ret = true;
    for(int idx = 0 ; idx < size ; ++idx)
    {
        int start = (std::max)(idx - semiWidth,0);
        int end = (std::min)(idx + semiWidth,size-1);
        float res = 0;
        for(int j = start ; j <= end ; ++j)
        {
            res+= values[j];
        }

        res /= float(end - start +1);

        if(std::abs(res-smoothValues[idx]) > 1e-3)
        {
            std::cout << "idx = " << idx << " -- expected = " << res << " -- result = " << smoothValues[idx] << std::endl;
            ret = false;
        }
    }

    return ret;
}

// generate a uniform law over [0,10]
float myRand()
{
    static const double divisor = double(RAND_MAX)+1.;
    return double(rand())/divisor * 10.;
}

int main()
{
    unsigned int size = 1024;
    // wSize must be odd
    unsigned int wSize = 21;
    // get the default device
    compute::device device = compute::system::default_device();
    // create a context for the device
    compute::context context(device);
    // get the program
    compute::program program = make_sma_program(context);

    // create vector of random numbers on the host
    std::vector<float> host_vector(size);
    std::vector<float> host_result(size);
    std::generate(host_vector.begin(), host_vector.end(), myRand);

    compute::vector<float> a(size,context);
    compute::vector<float> b(size,context);
    compute::vector<float> c(size,context);
    compute::command_queue queue(context, device);

    compute::copy(host_vector.begin(),host_vector.end(),a.begin(),queue);

    // scan values
    compute::inclusive_scan(a.begin(),a.end(),b.begin(),queue);
    // sma kernel
    compute::kernel kernel(program, "SMA");
    kernel.set_arg(0,b.get_buffer());
    kernel.set_arg(1,(int)b.size());
    kernel.set_arg(2,c.get_buffer());
    kernel.set_arg(3,(int)wSize);

    using compute::uint_;
    uint_ tpb = 128;
    uint_ workSize = size;
    queue.enqueue_1d_range_kernel(kernel,0,workSize,tpb);

    compute::copy(c.begin(),c.end(),host_result.begin(),queue);

    bool res = check_results(host_vector,host_result,wSize);
    std::string status = res ? "results are equivalent" : "GPU results differs from CPU one's";
    std::cout << status << std::endl;

    return 0;
}

