//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <cstdlib>
#include <iostream>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/copy_n.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/utility/source.hpp>

namespace compute = boost::compute;

// return a random float between lo and hi
float rand_float(float lo, float hi)
{
    float x = (float) std::rand() / (float) RAND_MAX;

    return (1.0f - x) * lo + x * hi;
}

// this example demostrates a black-scholes option pricing kernel.
int main()
{
    // number of options
    const int N = 4000000;

    // black-scholes parameters
    const float risk_free_rate = 0.02f;
    const float volatility = 0.30f;

    // get default device and setup context
    compute::device gpu = compute::system::default_device();
    compute::context context(gpu);
    compute::command_queue queue(context, gpu);
    std::cout << "device: " << gpu.name() << std::endl;

    // initialize option data on host
    std::vector<float> stock_price_data(N);
    std::vector<float> option_strike_data(N);
    std::vector<float> option_years_data(N);

    std::srand(5347);
    for(int i = 0; i < N; i++){
        stock_price_data[i] = rand_float(5.0f, 30.0f);
        option_strike_data[i] = rand_float(1.0f, 100.0f);
        option_years_data[i] = rand_float(0.25f, 10.0f);
    }

    // create memory buffers on the device
    compute::vector<float> call_result(N, context);
    compute::vector<float> put_result(N, context);
    compute::vector<float> stock_price(N, context);
    compute::vector<float> option_strike(N, context);
    compute::vector<float> option_years(N, context);

    // copy initial values to the device
    compute::copy_n(stock_price_data.begin(), N, stock_price.begin(), queue);
    compute::copy_n(option_strike_data.begin(), N, option_strike.begin(), queue);
    compute::copy_n(option_years_data.begin(), N, option_years.begin(), queue);

    // source code for black-scholes program
    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        // approximation of the cumulative normal distribution function
        static float cnd(float d)
        {
            const float A1 =  0.319381530f;
            const float A2 = -0.356563782f;
            const float A3 =  1.781477937f;
            const float A4 = -1.821255978f;
            const float A5 =  1.330274429f;
            const float RSQRT2PI = 0.39894228040143267793994605993438f;

            float K = 1.0f / (1.0f + 0.2316419f * fabs(d));
            float cnd =
                RSQRT2PI * exp(-0.5f * d * d) *
                (K * (A1 + K * (A2 + K * (A3 + K * (A4 + K * A5)))));

            if(d > 0){
                cnd = 1.0f - cnd;
            }

            return cnd;
        }

        // black-scholes option pricing kernel
        __kernel void black_scholes(__global float *call_result,
                                    __global float *put_result,
                                    __global const float *stock_price,
                                    __global const float *option_strike,
                                    __global const float *option_years,
                                    float risk_free_rate,
                                    float volatility)
        {
            const uint opt = get_global_id(0);

            float S = stock_price[opt];
            float X = option_strike[opt];
            float T = option_years[opt];
            float R = risk_free_rate;
            float V = volatility;

            float sqrtT = sqrt(T);
            float d1 = (log(S / X) + (R + 0.5f * V * V) * T) / (V * sqrtT);
            float d2 = d1 - V * sqrtT;
            float CNDD1 = cnd(d1);
            float CNDD2 = cnd(d2);

            float expRT = exp(-R * T);
            call_result[opt] = S * CNDD1 - X * expRT * CNDD2;
            put_result[opt] = X * expRT * (1.0f - CNDD2) - S * (1.0f - CNDD1);
        }
    );

    // build black-scholes program
    compute::program program = compute::program::create_with_source(source, context);
    program.build();

    // setup black-scholes kernel
    compute::kernel kernel(program, "black_scholes");
    kernel.set_arg(0, call_result);
    kernel.set_arg(1, put_result);
    kernel.set_arg(2, stock_price);
    kernel.set_arg(3, option_strike);
    kernel.set_arg(4, option_years);
    kernel.set_arg(5, risk_free_rate);
    kernel.set_arg(6, volatility);

    // execute black-scholes kernel
    queue.enqueue_1d_range_kernel(kernel, 0, N, 0);

    // print out the first option's put and call prices
    float call0, put0;
    compute::copy_n(put_result.begin(), 1, &put0, queue);
    compute::copy_n(call_result.begin(), 1, &call0, queue);

    std::cout << "option 0 call price: " << call0 << std::endl;
    std::cout << "option 0 put price: " << put0 << std::endl;

    // due to the differences in the random-number generators between Operating Systems
    // and/or compilers, we will get different "expected" results for this example
#ifdef __APPLE__
    double expected_call0 = 0.000249461;
    double expected_put0 = 26.2798;
#elif _MSC_VER
    double expected_call0 = 8.21412;
    double expected_put0 = 2.25904;
#else
    double expected_call0 = 0.0999f;
    double expected_put0 = 43.0524f;
#endif

    // check option prices
    if(std::abs(call0 - expected_call0) > 1e-4 || std::abs(put0 - expected_put0) > 1e-4){
        std::cerr << "error: option prices are wrong" << std::endl;
        return -1;
    }

    return 0;
}
