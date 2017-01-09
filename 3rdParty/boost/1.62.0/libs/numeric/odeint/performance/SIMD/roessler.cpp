/*
 * Simulation of an ensemble of Roessler attractors
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

namespace odeint = boost::numeric::odeint;

typedef boost::timer timer_type;

typedef double fp_type;
//typedef float fp_type;

typedef boost::array<fp_type, 3> state_type;
typedef std::vector<state_type> state_vec;

//---------------------------------------------------------------------------
struct roessler_system {
    const fp_type m_a, m_b, m_c;

    roessler_system(const fp_type a, const fp_type b, const fp_type c)
        : m_a(a), m_b(b), m_c(c)
    {}

    void operator()(const state_type &x, state_type &dxdt, const fp_type t) const
    {
        dxdt[0] = -x[1] - x[2];
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
//const size_t steps = 50;

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

state_vec state(n);
for(size_t i=0; i<n; ++i)
{
    state[i][0] = x[i];
    state[i][1] = y[i];
    state[i][2] = z[i];
}

std::cout.precision(16);

std::cout << "# n: " << n << std::endl;

std::cout << x[0] << std::endl;


// Stepper type - use never_resizer for slight performance improvement
odeint::runge_kutta4_classic<state_type, fp_type, state_type, fp_type,
                             odeint::array_algebra,
                             odeint::default_operations,
                             odeint::never_resizer> stepper;

roessler_system sys(a, b, c);

timer_type timer;

fp_type t = 0.0;

for (int step = 0; step < steps; step++)
{
    for(size_t i=0; i<n; ++i)
    {
        stepper.do_step(sys, state[i], t, dt);
    }
    t += dt;
}

std::cout << "Integration finished, runtime for " << steps << " steps: ";
std::cout << timer.elapsed() << " s" << std::endl;

// compute some accumulation to make sure all results have been computed
fp_type s = 0.0;
for(size_t i = 0; i < n; ++i)
{
    s += state[i][0];
}

std::cout << state[0][0] << std::endl;
std::cout << s/n << std::endl;

}
