/*
 [auto_generated]
 test/std_array.cpp

 [begin_description]
 Checks if odeint compiles fine with the std::array using the array algebra
 [end_description]

 Copyright 2009-2014 Karsten Ahnert
 Copyright 2009-2014 Mario Mulansky

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */


#define BOOST_TEST_MODULE odeint_std_array

#include <array>
#include <boost/numeric/odeint.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/test/unit_test.hpp>

using namespace boost::unit_test;

typedef std::array<double, 3> state_type;

void rhs(const state_type &x, state_type &dxdt, const double t)
{
}

BOOST_AUTO_TEST_SUITE( unwrap_reference_test )

BOOST_AUTO_TEST_CASE( test_case )
{
    state_type x = {0.0, 0.0, 0.0};

    typedef boost::numeric::odeint::runge_kutta4<state_type> stepper_type;
// check if array algebra is selected, but only if odeint detects c++11
#ifdef BOOST_NUMERIC_ODEINT_CXX11
    BOOST_STATIC_ASSERT(( boost::is_same< stepper_type::algebra_type , 
                          boost::numeric::odeint::array_algebra >::value ));
#endif
    stepper_type stepper1;
    stepper1.do_step(rhs, x, 0.0, 0.1);

    boost::numeric::odeint::runge_kutta4<
        state_type, double, state_type, double,
        boost::numeric::odeint::array_algebra > stepper;
    stepper.do_step(rhs, x, 0.0, 0.1);

}


BOOST_AUTO_TEST_SUITE_END()
