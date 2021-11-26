//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <iostream>

#include <Eigen/Core>
#include <Eigen/LU>

#include <boost/compute/function.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/types/fundamental.hpp>

namespace compute = boost::compute;

// this example shows how to compute the determinant of many 4x4 matrices
// using a determinant function and the transform() algorithm. in OpenCL the
// float16 type can be used to store a 4x4 matrix and the components are laid
// out in the following order:
//
// M = [ s0 s4 s8 sc ]
//     [ s1 s5 s9 sd ]
//     [ s2 s6 sa se ]
//     [ s3 s7 sb sf ]
//
// the input matrices are created using eigen's random matrix and then
// used again at the end to verify the results of the determinant function.
int main()
{
    // get default device and setup context
    compute::device gpu = compute::system::default_device();
    compute::context context(gpu);
    compute::command_queue queue(context, gpu);
    std::cout << "device: " << gpu.name() << std::endl;

    size_t n = 1000;

    // create random 4x4 matrices on the host
    std::vector<Eigen::Matrix4f> matrices(n);
    for(size_t i = 0; i < n; i++){
        matrices[i] = Eigen::Matrix4f::Random();
    }

    // copy matrices to the device
    using compute::float16_;
    compute::vector<float16_> input(n, context);
    compute::copy(
        matrices.begin(), matrices.end(), input.begin(), queue
    );

    // function returning the determinant of a 4x4 matrix.
    BOOST_COMPUTE_FUNCTION(float, determinant4x4, (const float16_ m),
    {
        return m.s0*m.s5*m.sa*m.sf + m.s0*m.s6*m.sb*m.sd + m.s0*m.s7*m.s9*m.se +
               m.s1*m.s4*m.sb*m.se + m.s1*m.s6*m.s8*m.sf + m.s1*m.s7*m.sa*m.sc +
               m.s2*m.s4*m.s9*m.sf + m.s2*m.s5*m.sb*m.sc + m.s2*m.s7*m.s8*m.sd +
               m.s3*m.s4*m.sa*m.sd + m.s3*m.s5*m.s8*m.se + m.s3*m.s6*m.s9*m.sc -
               m.s0*m.s5*m.sb*m.se - m.s0*m.s6*m.s9*m.sf - m.s0*m.s7*m.sa*m.sd -
               m.s1*m.s4*m.sa*m.sf - m.s1*m.s6*m.sb*m.sc - m.s1*m.s7*m.s8*m.se -
               m.s2*m.s4*m.sb*m.sd - m.s2*m.s5*m.s8*m.sf - m.s2*m.s7*m.s9*m.sc -
               m.s3*m.s4*m.s9*m.se - m.s3*m.s5*m.sa*m.sc - m.s3*m.s6*m.s8*m.sd;
    });

    // calculate determinants on the gpu
    compute::vector<float> determinants(n, context);
    compute::transform(
        input.begin(), input.end(), determinants.begin(), determinant4x4, queue
    );

    // check determinants
    std::vector<float> host_determinants(n);
    compute::copy(
        determinants.begin(), determinants.end(), host_determinants.begin(), queue
    );

    for(size_t i = 0; i < n; i++){
        float det = matrices[i].determinant();

        if(std::abs(det - host_determinants[i]) > 1e-6){
          std::cerr << "error: wrong determinant at " << i << " ("
                    << host_determinants[i] << " != " << det << ")"
                    << std::endl;
            return -1;
        }
    }

    return 0;
}
