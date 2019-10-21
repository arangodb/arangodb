/*
 
 [begin_description]
 Test case for issue 147
 [end_description]

 Copyright 2011-2015 Karsten Ahnert
 Copyright 2011-2015 Mario Mulansky

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */


// disable checked iterator warning for msvc

#include <boost/config.hpp>
#ifdef BOOST_MSVC
    #pragma warning(disable:4996)
#endif

#define BOOST_TEST_MODULE odeint_regression_147

#include <utility>

#include <boost/array.hpp>

#include <boost/test/unit_test.hpp>

#include <boost/mpl/vector.hpp>

#include <boost/numeric/odeint.hpp>

using namespace boost::unit_test;
using namespace boost::numeric::odeint;
namespace mpl = boost::mpl;

typedef double state_type;

void rhs( const state_type &x , state_type &dxdt , const double t )
{
    dxdt = 1;
}


template<class Stepper, class InitStepper>
struct perform_init_test
{
    void operator()( void )
    {
        double t = 0;
        const double dt = 0.1;

        state_type x = 0;

        Stepper stepper;
        InitStepper init_stepper;
        stepper.initialize( init_stepper, rhs, x, t, dt ); 

        // ab-stepper needs order-1 init steps: t and x should be (order-1)*dt
        BOOST_CHECK_CLOSE( t , (stepper.order()-1)*dt , 1E-16 );
        BOOST_CHECK_CLOSE( x, ( stepper.order() - 1 ) * dt, 2E-14 );
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


BOOST_AUTO_TEST_SUITE( regression_147_test )

BOOST_AUTO_TEST_CASE_TEMPLATE( init_test , InitStepper, 
                               runge_kutta_steppers )
{
    perform_init_test< adams_bashforth<4, state_type>, InitStepper > tester;
    tester();
}

BOOST_AUTO_TEST_SUITE_END()
