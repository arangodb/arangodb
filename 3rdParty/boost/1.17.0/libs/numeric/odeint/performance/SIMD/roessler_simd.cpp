/*
 * Simulation of an ensemble of Roessler attractors using NT2 SIMD library
 * This requires the SIMD library headers.
 *
 * Copyright 2014 Mario Mulansky
 *
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or
 * copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */


#include <iostream>
#include <vector>
#include <random>

#include <boost/timer.hpp>
#include <boost/array.hpp>

#include <boost/numeric/odeint.hpp>
#include <boost/simd/sdk/simd/pack.hpp>
#include <boost/simd/sdk/simd/io.hpp>
#include <boost/simd/memory/allocator.hpp>
#include <boost/simd/include/functions/splat.hpp>
#include <boost/simd/include/functions/plus.hpp>
#include <boost/simd/include/functions/multiplies.hpp>


namespace odeint = boost::numeric::odeint;
namespace simd = boost::simd;

typedef boost::timer timer_type;

static const size_t dim = 3;  // roessler is 3D

typedef double fp_type;
//typedef float fp_type;
 
typedef simd::pack<fp_type> simd_pack;
typedef boost::array<simd_pack, dim> state_type;
// use the simd allocator to get properly aligned memory
typedef std::vector< state_type, simd::allocator< state_type > > state_vec;

static const size_t pack_size = simd_pack::static_size;

//---------------------------------------------------------------------------
struct roessler_system {
    const fp_type m_a, m_b, m_c;

    roessler_system(const fp_type a, const fp_type b, const fp_type c)
        : m_a(a), m_b(b), m_c(c)
    {}

    void operator()(const state_type &x, state_type &dxdt, const fp_type t) const
    {
        dxdt[0] = -1.0*x[1] - x[2];
        dxdt[1] = x[0] + m_a * x[1];
        dxdt[2] = m_b + x[2] * (x[0] - m_c);
    }
};

//---------------------------------------------------------------------------
int main(int argc, char *argv[]) {
if(argc<3)
{
    std::cerr << "Expected size and steps as parameter" << std::endl;
    exit(1);
}
const size_t n = atoi(argv[1]);
const size_t steps = atoi(argv[2]);

const fp_type dt = 0.01;

const fp_type a = 0.2;
const fp_type b = 1.0;
const fp_type c = 9.0;

// random initial conditions on the device
std::vector<fp_type> x(n), y(n), z(n);
std::default_random_engine generator;
std::uniform_real_distribution<fp_type> distribution_xy(-8.0, 8.0);
std::uniform_real_distribution<fp_type> distribution_z(0.0, 20.0);
auto rand_xy = std::bind(distribution_xy, std::ref(generator));
auto rand_z = std::bind(distribution_z, std::ref(generator));
std::generate(x.begin(), x.end(), rand_xy);
std::generate(y.begin(), y.end(), rand_xy);
std::generate(z.begin(), z.end(), rand_z);

state_vec state(n/pack_size);
for(size_t i=0; i<n/pack_size; ++i)
{
    for(size_t p=0; p<pack_size; ++p)
    {
        state[i][0][p] = x[i*pack_size+p];
        state[i][1][p] = y[i*pack_size+p];
        state[i][2][p] = z[i*pack_size+p];
    }
}

std::cout << "Systems: " << n << std::endl;
std::cout << "Steps: " << steps << std::endl;
std::cout << "SIMD pack size: " << pack_size << std::endl;

std::cout << state[0][0] << std::endl;

// Stepper type
odeint::runge_kutta4_classic<state_type, fp_type, state_type, fp_type,
                             odeint::array_algebra, odeint::default_operations,
                             odeint::never_resizer> stepper;

roessler_system sys(a, b, c);

timer_type timer;

fp_type t = 0.0;

for(int step = 0; step < steps; step++)
{
    for(size_t i = 0; i < n/pack_size; ++i)
    {
        stepper.do_step(sys, state[i], t, dt);
    }
    t += dt;
}

std::cout.precision(16);

std::cout << "Integration finished, runtime for " << steps << " steps: ";
std::cout << timer.elapsed() << " s" << std::endl;

// compute some accumulation to make sure all results have been computed
simd_pack s_pack = 0.0;
for(size_t i = 0; i < n/pack_size; ++i)
{
    s_pack += state[i][0];
}

fp_type s = 0.0;
for(size_t p=0; p<pack_size; ++p)
{
    s += s_pack[p];
}


std::cout << state[0][0] << std::endl;
std::cout << s/n << std::endl;

}
