/*
 * abm_precision.cpp
 *
 * example to check the order of the multi-step methods
 *
 * Copyright 2009-2013 Karsten Ahnert
 * Copyright 2009-2013 Mario Mulansky
 *
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or
 * copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include <iostream>
#include <cmath>

#include <boost/array.hpp>
#include <boost/numeric/odeint.hpp>

using namespace boost::numeric::odeint;

const int Steps = 4;

typedef double value_type;

typedef boost::array< double , 2 > state_type;

typedef runge_kutta_fehlberg78<state_type> initializing_stepper_type;
typedef adams_bashforth_moulton< Steps , state_type > stepper_type;
//typedef adams_bashforth< Steps , state_type > stepper_type;

// harmonic oscillator, analytic solution x[0] = sin( t )
struct osc
{
    void operator()( const state_type &x , state_type &dxdt , const double t ) const
    {
        dxdt[0] = x[1];
        dxdt[1] = -x[0];
    }
};

int main()
{
    stepper_type stepper;
    initializing_stepper_type init_stepper;
    const int o = stepper.order()+1; //order of the error is order of approximation + 1

    const state_type x0 = {{ 0.0 , 1.0 }};
    state_type x1 = x0;
    double t = 0.0;
    double dt = 0.25;
    // initialization, does a number of steps already to fill internal buffer, t is increased
    // we use the rk78 as initializing stepper
    stepper.initialize( boost::ref(init_stepper) , osc() , x1 , t , dt );
    // do a number of steps to fill the buffer with results from adams bashforth
    for( size_t n=0 ; n < stepper.steps ; ++n )
    {
        stepper.do_step( osc() , x1 , t , dt );
        t += dt;
    }
    double A = std::sqrt( x1[0]*x1[0] + x1[1]*x1[1] );
    double phi = std::asin(x1[0]/A) - t;
    // now we do the actual step
    stepper.do_step( osc() , x1 , t , dt );
    // only examine the error of the adams-bashforth-moulton step, not the initialization
    const double f = 2.0 * std::abs( A*sin(t+dt+phi) - x1[0] ) / std::pow( dt , o ); // upper bound

    std::cout << "# " << o << " , " << f << std::endl;

    /* as long as we have errors above machine precision */
    while( f*std::pow( dt , o ) > 1E-16 )
    {
        x1 = x0;
        t = 0.0;
        stepper.initialize( boost::ref(init_stepper) , osc() , x1 , t , dt );
        A = std::sqrt( x1[0]*x1[0] + x1[1]*x1[1] );
        phi = std::asin(x1[0]/A) - t;
        // now we do the actual step
        stepper.do_step( osc() , x1 , t , dt );
        // only examine the error of the adams-bashforth-moulton step, not the initialization
        std::cout << dt << '\t' <<  std::abs( A*sin(t+dt+phi) - x1[0] ) << std::endl;
        dt *= 0.5;
    }
}
