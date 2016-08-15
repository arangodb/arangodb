/*
 [auto_generated]
 libs/numeric/odeint/test/integrate_overflow.cpp

 [begin_description]
 This file tests the overflow exception of the integrate functions.
 [end_description]

 Copyright 2015 Mario Mulansky

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */


#define BOOST_TEST_MODULE odeint_integrate_overflow

#include <boost/test/unit_test.hpp>

#include <utility>
#include <iostream>
#include <vector>


#include <vector>
#include <cmath>
#include <iostream>

#include <boost/iterator/counting_iterator.hpp>

#include <boost/numeric/odeint.hpp>


using namespace boost::numeric::odeint;


typedef double value_type;
typedef std::vector< value_type > state_type;

// the famous lorenz system as usual
void lorenz( const state_type &x , state_type &dxdt , const value_type t )
{
    //const value_type sigma( 10.0 );
    const value_type R( 28.0 );
    const value_type b( value_type( 8.0 ) / value_type( 3.0 ) );

    // first component trivial
    dxdt[0] = 1.0; //sigma * ( x[1] - x[0] );
    dxdt[1] = R * x[0] - x[1] - x[0] * x[2];
    dxdt[2] = -b * x[2] + x[0] * x[1];
}

struct push_back_time
{
    std::vector< double >& m_times;

    push_back_time( std::vector< double > &times )
            :  m_times( times ) { }

    void operator()( const state_type &x , double t )
    {
        m_times.push_back( t );
    }
};

typedef runge_kutta_dopri5<state_type> stepper_type;
typedef runge_kutta_cash_karp54<state_type> cash_karp_type;


BOOST_AUTO_TEST_SUITE( integrate_overflow )

BOOST_AUTO_TEST_CASE( test_integrate_const )
{
    state_type x(3, 5.0);
    std::vector<double> times;

    // check the function signatures with normal stepper
    integrate_const(stepper_type(), lorenz, x, 0.0, 10.0, 1.0);
    integrate_const(stepper_type(), lorenz, x, 0.0, 10.0, 1.0, null_observer());
    // no exceptions expected for normal steppers
    integrate_const(stepper_type(), lorenz, x, 0.0, 10.0, 1.0, null_observer(), max_step_checker(10));


    // check exceptions for controlled stepper
    integrate_const(make_controlled<stepper_type>(1E-5, 1E-5), lorenz, x, 0.0, 10.0, 1.0);
    // very small error terms -> standard overflow threshold of 500 should fire an exception
    BOOST_CHECK_THROW(integrate_const(make_controlled<stepper_type>(1E-15, 1E-15), lorenz, x,
                                      0.0, 10.0, 1.0, push_back_time(times), max_step_checker()),
                      std::runtime_error);
    // small threshold of 10 -> larger error still gives an exception
    BOOST_CHECK_THROW(integrate_const(make_controlled<stepper_type>(1E-5, 1E-5), lorenz, x,
                                      0.0, 10.0, 1.0, push_back_time(times), max_step_checker(10)),
                      std::runtime_error);

    // check exceptions for dense output stepper
    integrate_const(make_dense_output<stepper_type>(1E-5, 1E-5), lorenz, x, 0.0, 10.0, 1.0);
    integrate_const(make_dense_output<stepper_type>(1E-5, 1E-5), lorenz, x, 0.0, 10.0, 1.0);
    // very small error terms -> standard overflow threshold of 500 should fire an exception
    BOOST_CHECK_THROW(integrate_const(make_dense_output<stepper_type>(1E-15, 1E-15), lorenz, x,
                                      0.0, 10.0, 1.0, push_back_time(times), max_step_checker()),
                      std::runtime_error);
    // small threshold of 10 -> larger error still gives an exception
    BOOST_CHECK_THROW(integrate_const(make_dense_output<stepper_type>(1E-5, 1E-5), lorenz, x,
                                      0.0, 10.0, 1.0, push_back_time(times), max_step_checker(10)),
                      std::runtime_error);
}

BOOST_AUTO_TEST_CASE( test_integrate_n_steps )
{
    state_type x(3, 5.0);
    std::vector<double> times;

    // check the function signatures with normal stepper
    integrate_n_steps(stepper_type(), lorenz, x, 0.0, 1.0, 10);
    integrate_n_steps(stepper_type(), lorenz, x, 0.0, 1.0, 10, null_observer());
    // no exceptions expected for normal steppers
    integrate_n_steps(stepper_type(), lorenz, x, 0.0, 1.0, 10, null_observer(), max_step_checker(10));


    // check exceptions for controlled stepper
    integrate_n_steps(make_controlled<stepper_type>(1E-5, 1E-5), lorenz, x, 0.0, 1.0, 10);
    // very small error terms -> standard overflow threshold of 500 should fire an exception
    BOOST_CHECK_THROW(integrate_n_steps(make_controlled<stepper_type>(1E-15, 1E-15), lorenz, x,
                                        0.0, 1.0, 10, push_back_time(times), max_step_checker()),
                      std::runtime_error);
    // small threshold of 10 -> larger error still gives an exception
    BOOST_CHECK_THROW(integrate_n_steps(make_controlled<stepper_type>(1E-5, 1E-5), lorenz, x,
                                        0.0, 1.0, 10, push_back_time(times), max_step_checker(10)),
                      std::runtime_error);

    // check exceptions for dense output stepper
    integrate_n_steps(make_dense_output<stepper_type>(1E-5, 1E-5), lorenz, x, 0.0, 1.0, 10);
    integrate_n_steps(make_dense_output<stepper_type>(1E-5, 1E-5), lorenz, x, 0.0, 1.0, 10, push_back_time(times));
    // very small error terms -> standard overflow threshold of 500 should fire an exception
    BOOST_CHECK_THROW(integrate_n_steps(make_dense_output<stepper_type>(1E-15, 1E-15), lorenz, x,
                                        0.0, 1.0, 10, push_back_time(times), max_step_checker()),
                      std::runtime_error);
    // small threshold of 10 -> larger error still gives an exception
    BOOST_CHECK_THROW(integrate_n_steps(make_dense_output<stepper_type>(1E-5, 1E-5), lorenz, x,
                                        0.0, 1.0, 10, push_back_time(times), max_step_checker(10)),
                      std::runtime_error);
}

BOOST_AUTO_TEST_CASE( test_integrate_times )
{
    state_type x(3, 5.0);
    std::vector<double> times;
    boost::counting_iterator<int> t0(0);
    boost::counting_iterator<int> t1(10);

    // check the function signatures with normal stepper
    integrate_times(stepper_type(), lorenz, x, t0, t1, 1.0 , push_back_time(times));
    // no exceptions expected for big enough step size
    integrate_times(stepper_type(), lorenz, x, t0, t1, 1.0 , push_back_time(times), max_step_checker(10));
    // if dt*max_steps < observer time difference we expect an exception
    BOOST_CHECK_THROW(integrate_times(stepper_type(), lorenz, x, t0, t1, 0.01, push_back_time(times),
                                      max_step_checker(10)),
                      std::runtime_error);

    // check exceptions for controlled stepper
    // no exception if no checker is provided
    integrate_times(make_controlled<stepper_type>(1E-5, 1E-5), lorenz, x, t0, t1, 1.0 , push_back_time(times));
    // very small error terms -> standard overflow threshold of 500 should fire an exception
    BOOST_CHECK_THROW(integrate_times(make_controlled<stepper_type>(1E-15, 1E-15), lorenz, x,
                                      t0, t1, 1.0 , push_back_time(times), max_step_checker()),
                      std::runtime_error);

    // small threshold of 10 -> larger error still gives an exception
    BOOST_CHECK_THROW(integrate_times(make_controlled<stepper_type>(1E-5, 1E-5), lorenz, x,
                                      t0, t1, 1.0 , push_back_time(times), max_step_checker(10)),
                      std::runtime_error);

    // check cash karp controlled stepper
    // no exception if no checker is provided
    integrate_times(make_controlled<cash_karp_type>(1E-5, 1E-5), lorenz, x, t0, t1, 1.0 , push_back_time(times));
    // very small error terms -> standard overflow threshold of 500 should fire an exception
    BOOST_CHECK_THROW(integrate_times(make_controlled<cash_karp_type>(1E-15, 1E-15), lorenz, x,
                      t0, t1, 1.0 , push_back_time(times), max_step_checker()),
    std::runtime_error);

    // small threshold of 10 -> larger error still gives an exception
    BOOST_CHECK_THROW(integrate_times(make_controlled<cash_karp_type>(1E-5, 1E-5), lorenz, x,
                      t0, t1, 1.0 , push_back_time(times), max_step_checker(10)),
    std::runtime_error);

    // check exceptions for dense output stepper
    integrate_times(make_dense_output<stepper_type>(1E-5, 1E-5), lorenz, x, t0, t1, 1.0 ,
                    push_back_time(times));
    // very small error terms -> standard overflow threshold of 500 should fire an exception
    BOOST_CHECK_THROW(integrate_times(make_dense_output<stepper_type>(1E-15, 1E-15), lorenz, x,
                                      t0, t1, 1.0 , push_back_time(times), max_step_checker()),
                      std::runtime_error);
    // small threshold of 10 -> larger error still gives an exception
    BOOST_CHECK_THROW(integrate_times(make_dense_output<stepper_type>(1E-5, 1E-5), lorenz, x,
                                      t0, t1, 1.0 , push_back_time(times), max_step_checker(10)),
                      std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()