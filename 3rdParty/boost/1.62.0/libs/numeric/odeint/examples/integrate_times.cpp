/* Boost libs/numeric/odeint/examples/integrate_times.cpp

 Copyright 2009-2014 Karsten Ahnert
 Copyright 2009-2014 Mario Mulansky

 example for the use of integrate_times

 Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */


#include <iostream>
#include <boost/numeric/odeint.hpp>

using namespace std;
using namespace boost::numeric::odeint;


/*
 * simple 1D ODE
 */

void rhs( const double x , double &dxdt , const double t )
{
    dxdt = 3.0/(2.0*t*t) + x/(2.0*t);
}

void write_cout( const double &x , const double t )
{
    cout << t << '\t' << x << endl;
}

// state_type = double
typedef runge_kutta_dopri5< double > stepper_type;

const double dt = 0.1;

int main()
{
    // create a vector with observation time points
    std::vector<double> times( 91 );
    for( size_t i=0 ; i<times.size() ; ++i )
        times[i] = 1.0 + dt*i;

    double x = 0.0; //initial value x(1) = 0
    // we can provide the observation time as a boost range (i.e. the vector)
    integrate_times( make_controlled( 1E-12 , 1E-12 , stepper_type() ) , rhs ,
                     x , times , dt , write_cout );
    // or as two iterators
    //integrate_times( make_controlled( 1E-12 , 1E-12 , stepper_type() ) , rhs ,
    //                   x , times.begin() , times.end() , dt , write_cout );
}
