/*
 
 [begin_description]
 Test case for issue 149: 
 Error C2582 with msvc-10 when using iterator-based integration
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
#include <iostream>

#include <boost/array.hpp>

#include <boost/test/unit_test.hpp>

#include <boost/mpl/vector.hpp>
#include <boost/range/algorithm/find_if.hpp>

#include <boost/numeric/odeint.hpp>
#include <boost/numeric/odeint/algebra/fusion_algebra.hpp>
#include <boost/numeric/odeint/algebra/fusion_algebra_dispatcher.hpp>


#include <boost/units/systems/si/length.hpp>
#include <boost/units/systems/si/time.hpp>
#include <boost/units/systems/si/velocity.hpp>
#include <boost/units/systems/si/acceleration.hpp>
#include <boost/units/systems/si/io.hpp>

#include <boost/fusion/container.hpp>


using namespace boost::unit_test;
using namespace boost::numeric::odeint;
namespace mpl = boost::mpl;

namespace fusion = boost::fusion;
namespace units = boost::units;
namespace si = boost::units::si;

typedef units::quantity< si::time , double > time_type;
typedef units::quantity< si::length , double > length_type;
typedef units::quantity< si::velocity , double > velocity_type;
typedef units::quantity< si::acceleration , double > acceleration_type;
typedef units::quantity< si::frequency , double > frequency_type;

typedef fusion::vector< length_type , velocity_type > state_type;
typedef fusion::vector< velocity_type , acceleration_type > deriv_type;


struct oscillator
{
    frequency_type m_omega;

    oscillator( const frequency_type &omega = 1.0 * si::hertz ) : m_omega( omega ) { }

    void operator()( const state_type &x , deriv_type &dxdt , time_type t ) const
    {
        fusion::at_c< 0 >( dxdt ) = fusion::at_c< 1 >( x );
        fusion::at_c< 1 >( dxdt ) = - m_omega * m_omega * fusion::at_c< 0 >( x );
    }
};


BOOST_AUTO_TEST_CASE( regression_168 )
{
    typedef runge_kutta_dopri5< state_type , double , deriv_type , time_type > stepper_type;

    state_type x( 1.0 * si::meter , 0.0 * si::meter_per_second );

    integrate_const( make_dense_output( 1.0e-6 , 1.0e-6 , stepper_type() ) , oscillator( 2.0 * si::hertz ) ,
                     x , 0.0 * si::second , 100.0 * si::second , 0.1 * si::second);
}