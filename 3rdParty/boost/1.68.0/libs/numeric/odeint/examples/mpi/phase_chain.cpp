/*
 * phase_chain.cpp
 *
 * Example of MPI parallelization with odeint
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
//[phase_chain_mpi_header
#include <boost/numeric/odeint.hpp>
#include <boost/numeric/odeint/external/mpi/mpi.hpp>
//]

using namespace std;
using namespace boost::numeric::odeint;
using boost::timer::cpu_timer;
using boost::math::double_constants::pi;

//[phase_chain_state
typedef mpi_state< vector<double> > state_type;
//]

//[phase_chain_mpi_rhs
struct phase_chain_mpi_state
{
    phase_chain_mpi_state( double gamma = 0.5 )
    : m_gamma( gamma ) { }

    void operator()( const state_type &x , state_type &dxdt , double /* t */ ) const
    {
        const size_t M = x().size();
        const bool have_left = x.world.rank() > 0,
                   have_right = x.world.rank() < x.world.size()-1;
        double x_left, x_right;
        boost::mpi::request r_left, r_right;
        if( have_left )
        {
            x.world.isend( x.world.rank()-1 , 0 , x().front() ); // send to x_right
            r_left = x.world.irecv( x.world.rank()-1 , 0 , x_left ); // receive from x().back()
        }
        if( have_right )
        {
            x.world.isend( x.world.rank()+1 , 0 , x().back() ); // send to x_left
            r_right = x.world.irecv( x.world.rank()+1 , 0 , x_right ); // receive from x().front()
        }
        for(size_t m = 1 ; m < M-1 ; ++m)
        {
            dxdt()[m] = coupling_func( x()[m+1] - x()[m] ) +
                        coupling_func( x()[m-1] - x()[m] );
        }
        dxdt()[0] = coupling_func( x()[1] - x()[0] );
        if( have_left )
        {
            r_left.wait();
            dxdt()[0] += coupling_func( x_left - x().front() );
        }
        dxdt()[M-1] = coupling_func( x()[M-2] - x()[M-1] );
        if( have_right )
        {
            r_right.wait();
            dxdt()[M-1] += coupling_func( x_right - x().back() );
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
    //[phase_chain_mpi_init
    boost::mpi::environment env( argc , argv );
    boost::mpi::communicator world;

    const size_t N = 131101;
    vector<double> x;
    if( world.rank() == 0 )
    {
        x.resize( N );
        boost::random::uniform_real_distribution<double> distribution( 0.0 , 2.0*pi );
        boost::random::mt19937 engine( 0 );
        generate( x.begin() , x.end() , boost::bind( distribution , engine ) );
    }

    state_type x_split( world );
    split( x , x_split );
    //]


    cpu_timer timer;
    //[phase_chain_mpi_integrate
    integrate_n_steps( runge_kutta4<state_type>() , phase_chain_mpi_state( 1.2 ) ,
                       x_split , 0.0 , 0.01 , 100 );
    unsplit( x_split , x );
    //]

    if( world.rank() == 0 )
    {
        double run_time = static_cast<double>(timer.elapsed().wall) * 1.0e-9;
        std::cerr << run_time << "s" << std::endl;
        // copy(x.begin(), x.end(), ostream_iterator<double>(cout, "\n"));
    }

    return 0;
}
