/*
 * Copyright 2011-2013 Mario Mulansky
 * Copyright 2012 Karsten Ahnert
 *
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or
 * copy at http://www.boost.org/LICENSE_1_0.txt)
 */


#include <iostream>

#include <boost/numeric/odeint.hpp>
#include <boost/numeric/ublas/vector.hpp>

typedef boost::numeric::ublas::vector< double > state_type;

void lorenz( const state_type &x , state_type &dxdt , const double t )
{
    const double sigma( 10.0 );
    const double R( 28.0 );
    const double b( 8.0 / 3.0 );
    
    dxdt[0] = sigma * ( x[1] - x[0] );
    dxdt[1] = R * x[0] - x[1] - x[0] * x[2];
    dxdt[2] = -b * x[2] + x[0] * x[1];
}

using namespace boost::numeric::odeint;

//[ublas_main
int main()
{
    state_type x(3);
    x[0] = 10.0; x[1] = 5.0 ; x[2] = 0.0;
    typedef runge_kutta_dopri5< state_type > stepper;
    integrate_const( make_dense_output< stepper >( 1E-6 , 1E-6 ) , lorenz , x ,
                     0.0 , 10.0 , 0.1 );
}
//]
