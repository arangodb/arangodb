/* Boost libs/numeric/odeint/examples/multiprecision/cmp_precision.cpp

 Copyright 2013 Karsten Ahnert
 Copyright 2013 Mario Mulansky

 example comparing double to multiprecision using Boost.Multiprecision

 Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */


#include <iostream>
#include <boost/numeric/odeint.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

using namespace std;
using namespace boost::numeric::odeint;

typedef boost::multiprecision::cpp_dec_float_50 mp_50;

/* we solve the simple ODE x' = 3/(2t^2) + x/(2t)
 * with initial condition x(1) = 0.
 * Analytic solution is x(t) = sqrt(t) - 1/t
 */

void rhs_m( const mp_50 x , mp_50 &dxdt , const mp_50 t )
{   // version for multiprecision
    dxdt = mp_50(3)/(mp_50(2)*t*t) + x/(mp_50(2)*t);
}

void rhs_d( const double x , double &dxdt , const double t )
{   // version for double precision
    dxdt = 3.0/(2.0*t*t) + x/(2.0*t);
}

// state_type = mp_50 = deriv_type = time_type = mp_50
typedef runge_kutta4< mp_50 , mp_50 , mp_50 , mp_50 , vector_space_algebra , default_operations , never_resizer > stepper_type_m;

typedef runge_kutta4< double , double , double , double , vector_space_algebra , default_operations , never_resizer > stepper_type_d;

int main()
{

    stepper_type_m stepper_m;
    stepper_type_d stepper_d;

    mp_50 dt_m( 0.5 );
    double dt_d( 0.5 );

    cout << "dt" << '\t' << "mp" << '\t' << "double" << endl;
    
    while( dt_m > 1E-20 )
    {

        mp_50 x_m = 0; //initial value x(1) = 0
        stepper_m.do_step( rhs_m , x_m , mp_50( 1 ) , dt_m );
        double x_d = 0;
        stepper_d.do_step( rhs_d , x_d , 1.0 , dt_d );        

        cout << dt_m << '\t';
        cout << abs((x_m - (sqrt(1+dt_m)-mp_50(1)/(1+dt_m)))/x_m) << '\t' ;
        cout << abs((x_d - (sqrt(1+dt_d)-mp_50(1)/(1+dt_d)))/x_d) << endl ;
        dt_m /= 2;
        dt_d /= 2;
    }
}
