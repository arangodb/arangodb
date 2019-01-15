/*
 * phase_chain_omp_state.cpp
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
#include <omp.h>
#include <boost/numeric/odeint.hpp>
#include <boost/numeric/odeint/external/openmp/openmp.hpp>

#include <boost/numeric/odeint.hpp>

using namespace std;
using namespace boost::numeric::odeint;
using boost::timer::cpu_timer;
using boost::math::double_constants::pi;

typedef openmp_state<double> state_type;

//[phase_chain_state_rhs
struct phase_chain_omp_state
{
    phase_chain_omp_state( double gamma = 0.5 )
    : m_gamma( gamma ) { }

    void operator()( const state_type &x , state_type &dxdt , double /* t */ ) const
    {
        const size_t N = x.size();
        #pragma omp parallel for schedule(runtime)
        for(size_t n = 0 ; n < N ; ++n)
        {
            const size_t M = x[n].size();
            for(size_t m = 1 ; m < M-1 ; ++m)
            {
                dxdt[n][m] = coupling_func( x[n][m+1] - x[n][m] ) +
                             coupling_func( x[n][m-1] - x[n][m] );
            }
            dxdt[n][0] = coupling_func( x[n][1] - x[n][0] );
            if( n > 0 )
            {
                dxdt[n][0] += coupling_func( x[n-1].back() - x[n].front() );
            }
            dxdt[n][M-1] = coupling_func( x[n][M-2] - x[n][M-1] );
            if( n < N-1 )
            {
                dxdt[n][M-1] += coupling_func( x[n+1].front() - x[n].back() );
            }
        }
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
    //[phase_chain_state_init
    const size_t N = 131101;
    vector<double> x( N );
    boost::random::uniform_real_distribution<double> distribution( 0.0 , 2.0*pi );
    boost::random::mt19937 engine( 0 );
    generate( x.begin() , x.end() , boost::bind( distribution , engine ) );
    const size_t blocks = omp_get_max_threads();
    state_type x_split( blocks );
    split( x , x_split );
    //]


    cpu_timer timer;
    //[phase_chain_state_integrate
    integrate_n_steps( runge_kutta4<state_type>() , phase_chain_omp_state( 1.2 ) ,
                       x_split , 0.0 , 0.01 , 100 );
    unsplit( x_split , x );
    //]

    double run_time = static_cast<double>(timer.elapsed().wall) * 1.0e-9;
    std::cerr << run_time << "s" << std::endl;
    // copy(x.begin(), x.end(), ostream_iterator<double>(cout, "\n"));

    return 0;
}
