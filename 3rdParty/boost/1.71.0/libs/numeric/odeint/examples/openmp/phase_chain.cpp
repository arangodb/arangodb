/*
 * phase_chain.cpp
 *
 * Example of OMP parallelization with odeint
 *
 * Copyright 2013 Karsten Ahnert
 * Copyright 2013 Mario Mulansky
 * Copyright 2013 Pascal Germroth
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <iostream>
#include <vector>
#include <boost/random.hpp>
#include <boost/timer/timer.hpp>
//[phase_chain_openmp_header
#include <omp.h>
#include <boost/numeric/odeint.hpp>
#include <boost/numeric/odeint/external/openmp/openmp.hpp>
//]

using namespace std;
using namespace boost::numeric::odeint;
using boost::timer::cpu_timer;
using boost::math::double_constants::pi;

//[phase_chain_vector_state
typedef std::vector< double > state_type;
//]

//[phase_chain_rhs
struct phase_chain
{
    phase_chain( double gamma = 0.5 )
    : m_gamma( gamma ) { }

    void operator()( const state_type &x , state_type &dxdt , double /* t */ ) const
    {
        const size_t N = x.size();
        #pragma omp parallel for schedule(runtime)
        for(size_t i = 1 ; i < N - 1 ; ++i)
        {
            dxdt[i] = coupling_func( x[i+1] - x[i] ) +
                      coupling_func( x[i-1] - x[i] );
        }
        dxdt[0  ] = coupling_func( x[1  ] - x[0  ] );
        dxdt[N-1] = coupling_func( x[N-2] - x[N-1] );
    }

    double coupling_func( double x ) const
    {
        return sin( x ) - m_gamma * ( 1.0 - cos( x ) );
    }

    double m_gamma;
};
//]


int main( int argc , char **argv )
{
    //[phase_chain_init
    size_t N = 131101;
    state_type x( N );
    boost::random::uniform_real_distribution<double> distribution( 0.0 , 2.0*pi );
    boost::random::mt19937 engine( 0 );
    generate( x.begin() , x.end() , boost::bind( distribution , engine ) );
    //]

    //[phase_chain_stepper
    typedef runge_kutta4<
                      state_type , double ,
                      state_type , double ,
                      openmp_range_algebra
                    > stepper_type;
    //]

    //[phase_chain_scheduling
    int chunk_size = N/omp_get_max_threads();
    omp_set_schedule( omp_sched_static , chunk_size );
    //]

    cpu_timer timer;
    //[phase_chain_integrate
    integrate_n_steps( stepper_type() , phase_chain( 1.2 ) ,
                       x , 0.0 , 0.01 , 100 );
    //]
    double run_time = static_cast<double>(timer.elapsed().wall) * 1.0e-9;
    std::cerr << run_time << "s" << std::endl;
    // copy(x.begin(), x.end(), ostream_iterator<double>(cout, "\n"));

    return 0;
}
