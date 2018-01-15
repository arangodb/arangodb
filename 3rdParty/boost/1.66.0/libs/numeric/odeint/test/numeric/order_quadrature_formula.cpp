/* Boost numeric test for orders of quadrature formulas test file

 Copyright 2015 Gregor de Cillia
 Copyright 2015 Mario Mulansky <mario.mulansky@gmx.net>

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
*/

// disable checked iterator warning for msvc
#include <boost/config.hpp>

#ifdef BOOST_MSVC
    #pragma warning(disable:4996)
#endif

#define BOOST_TEST_MODULE order_quadrature_formula

#include <iostream>
#include <cmath>

#include "boost/format.hpp"

#include <boost/test/unit_test.hpp>

#include <boost/mpl/vector.hpp>

#include <boost/numeric/odeint.hpp>

#include <boost/numeric/ublas/vector.hpp>

#include <boost/format.hpp>

using namespace boost::unit_test;
using namespace boost::numeric::odeint;
namespace mpl = boost::mpl;

typedef double value_type;
typedef value_type time_type;
typedef value_type state_type;

BOOST_AUTO_TEST_SUITE( order_of_convergence_test )

/* defines the simple monomial f(t) = (p+1) * t^p.*/
struct monomial
{
    int power;

    monomial(int p = 0) : power( p ){};

    void operator()( const state_type &x , state_type &dxdt , const time_type t )
    {
        dxdt = ( 1.0 + power ) * pow( t, power );
    }
};


/* generic test for all steppers that support integrate_const */
template< class Stepper >
struct stepper_order_test
{
    void operator()( int steps = 1 )
    {
        const int estimated_order = estimate_order( steps );
        const int defined_order = Stepper::order_value;

        std::cout << boost::format( "%-20i%-20i\n" )
	    % estimated_order %  defined_order;

        BOOST_REQUIRE_EQUAL( estimated_order, defined_order );
    }

    /*
    the order of the stepper is estimated by trying to solve the ODE
    x'(t) = (p+1) * t^p
    until the errors are too big to be justified by finite precision.
    the first value p for which the problem is *not* solved within the
    finite precision tolerance is the estimate for the order of the scheme.
     */
    int estimate_order( int steps )
    {
        const double dt = 1.0/steps;
        const double tolerance = steps*1E-15;
        int p;
        for( p = 0; true; p++ )
        {
            // begin with x'(t) = t^0 = 1
            //         => x (t) = t
            // then use   x'(t) = 2*t^1
            //         => x (t) = t^2
            // ...
            state_type x = 0.0;

            double t = integrate_n_steps( Stepper(), monomial( p ), x, 0.0, dt,
                                          steps );
            if( fabs( x - pow( t, ( 1.0 + p ) ) ) > tolerance )
                break;
        }
        // the smallest power p for which the test failed is the estimated order,
        // as the solution for this power is x(t) = t^{p+1}
        return p;
    }
};


typedef mpl::vector<
    euler< state_type > ,
    modified_midpoint< state_type > ,
    runge_kutta4< state_type > ,
    runge_kutta4_classic< state_type > ,
    runge_kutta_cash_karp54_classic< state_type > ,
    runge_kutta_cash_karp54< state_type > ,
    runge_kutta_dopri5< state_type > ,
    runge_kutta_fehlberg78< state_type >
    > runge_kutta_steppers;

typedef mpl::vector<
    adams_bashforth< 2, state_type, double, state_type, double,
                     vector_space_algebra, default_operations,
                     initially_resizer, runge_kutta_fehlberg78< state_type > >,
    adams_bashforth< 3, state_type, double, state_type, double,
                     vector_space_algebra, default_operations,
                     initially_resizer, runge_kutta_fehlberg78< state_type > >,
    adams_bashforth< 4, state_type, double, state_type, double,
                     vector_space_algebra, default_operations,
                     initially_resizer, runge_kutta_fehlberg78< state_type > >,
    adams_bashforth< 5, state_type, double, state_type, double,
                     vector_space_algebra, default_operations,
                     initially_resizer, runge_kutta_fehlberg78< state_type > >,
    adams_bashforth< 6, state_type, double, state_type, double,
                     vector_space_algebra, default_operations,
                     initially_resizer, runge_kutta_fehlberg78< state_type > >,
    adams_bashforth< 7, state_type, double, state_type, double,
                     vector_space_algebra, default_operations,
                     initially_resizer, runge_kutta_fehlberg78< state_type > >,
    adams_bashforth< 8, state_type, double, state_type, double,
                     vector_space_algebra, default_operations,
                     initially_resizer, runge_kutta_fehlberg78< state_type > >
    > ab_steppers;


typedef mpl::vector<
    adams_bashforth_moulton< 2, state_type, double, state_type, double,
                             vector_space_algebra, default_operations,
                             initially_resizer,
                             runge_kutta_fehlberg78< state_type > >,
    adams_bashforth_moulton< 3, state_type, double, state_type, double,
                             vector_space_algebra, default_operations,
                             initially_resizer,
                             runge_kutta_fehlberg78< state_type > >,
    adams_bashforth_moulton< 4, state_type, double, state_type, double,
                             vector_space_algebra, default_operations,
                             initially_resizer,
                             runge_kutta_fehlberg78< state_type > >,
    adams_bashforth_moulton< 5, state_type, double, state_type, double,
                             vector_space_algebra, default_operations,
                             initially_resizer,
                             runge_kutta_fehlberg78< state_type > >,
    adams_bashforth_moulton< 6, state_type, double, state_type, double,
                             vector_space_algebra, default_operations,
                             initially_resizer,
                             runge_kutta_fehlberg78< state_type > >,
    adams_bashforth_moulton< 7, state_type, double, state_type, double,
                             vector_space_algebra, default_operations,
                             initially_resizer,
                             runge_kutta_fehlberg78< state_type > >,
    adams_bashforth_moulton< 8, state_type, double, state_type, double,
                             vector_space_algebra, default_operations,
                             initially_resizer,
                             runge_kutta_fehlberg78< state_type > >
    > abm_steppers;


BOOST_AUTO_TEST_CASE_TEMPLATE( runge_kutta_test , Stepper, runge_kutta_steppers )
{
    stepper_order_test< Stepper > tester;
    tester(10);
}


BOOST_AUTO_TEST_CASE_TEMPLATE( adams_bashforth_test , Stepper, ab_steppers )
{
    stepper_order_test< Stepper > tester;
    tester(16);
}


BOOST_AUTO_TEST_CASE_TEMPLATE( adams_bashforth_moultion_test , Stepper, abm_steppers )
{
    stepper_order_test< Stepper > tester;
    tester(16);
}

BOOST_AUTO_TEST_SUITE_END()
