/*
 [auto_generated]
 libs/numeric/odeint/test/velocity_verlet.cpp

 [begin_description]
 tba.
 [end_description]

 Copyright 2009-2012 Karsten Ahnert
 Copyright 2009-2012 Mario Mulansky

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/config.hpp>
#ifdef BOOST_MSVC
    #pragma warning(disable:4996)
#endif

#define BOOST_TEST_MODULE odeint_velocity_verlet

#define BOOST_FUSION_INVOKE_MAX_ARITY 15
#define BOOST_RESULT_OF_NUM_ARGS 15

#include <boost/numeric/odeint/config.hpp>

#include "resizing_test_state_type.hpp"

#include <boost/numeric/odeint/stepper/velocity_verlet.hpp>
#include <boost/numeric/odeint/algebra/fusion_algebra.hpp>

#include <boost/array.hpp>
#include <boost/test/unit_test.hpp>

#include <boost/units/systems/si/length.hpp>
#include <boost/units/systems/si/time.hpp>
#include <boost/units/systems/si/velocity.hpp>
#include <boost/units/systems/si/acceleration.hpp>
#include <boost/units/systems/si/io.hpp>

#include <boost/fusion/include/vector.hpp>
#include <boost/fusion/include/vector20.hpp>
#include <boost/fusion/container.hpp>

namespace fusion = boost::fusion;
namespace units = boost::units;
namespace si = boost::units::si;

typedef double value_type;
typedef units::quantity< si::time , value_type > time_type;
typedef units::unit< units::derived_dimension< units::time_base_dimension , 2 >::type , si::system > time_2;
typedef units::quantity< time_2 , value_type > time_2_type;
typedef units::quantity< si::length , value_type > length_type;
typedef units::quantity< si::velocity , value_type > velocity_type;
typedef units::quantity< si::acceleration , value_type > acceleration_type;
typedef fusion::vector< length_type , length_type > coor_vector;
typedef fusion::vector< velocity_type , velocity_type > velocity_vector;
typedef fusion::vector< acceleration_type , acceleration_type > accelartion_vector;



using namespace boost::unit_test;
using namespace boost::numeric::odeint;

size_t ode_call_count;

struct velocity_verlet_fixture
{
    velocity_verlet_fixture( void ) { ode_call_count = 0; adjust_size_count = 0; }
};

struct ode
{
    template< class CoorIn , class MomentumIn , class AccelerationOut , class Time >
    void operator()( const CoorIn &q , const MomentumIn &p , AccelerationOut &a , Time t ) const
    {
        a[0] = -q[0] - p[0];
        a[1] = -q[1] - p[1];
        ++ode_call_count;
    }
};

struct ode_units
{
    void operator()( coor_vector const &q , velocity_vector const &p , accelartion_vector &a , time_type t ) const
    {
        const units::quantity< si::frequency , value_type > omega = 1.0 * si::hertz;
        const units::quantity< si::frequency , value_type > friction = 0.001 * si::hertz;
        fusion::at_c< 0 >( a ) = omega * omega * fusion::at_c< 0 >( q ) - friction * fusion::at_c< 0 >( p );
        fusion::at_c< 1 >( a ) = omega * omega * fusion::at_c< 1 >( q ) - friction * fusion::at_c< 0 >( p );
        ++ode_call_count;
    }
};

template< class Q , class P >
void init_state( Q &q , P &p )
{
    q[0] = 1.0 ; q[1] = 0.5;
    p[0] = 2.0 ; p[1] = -1.0;
}

typedef boost::array< double , 2 > array_type;
typedef std::vector< double > vector_type;

typedef velocity_verlet< array_type > array_stepper;
typedef velocity_verlet< vector_type > vector_stepper;

template< typename Resizer >
struct get_resizer_test_stepper
{
    typedef velocity_verlet< test_array_type , test_array_type , double , test_array_type , 
        double , double , range_algebra , default_operations , Resizer > type;
};





BOOST_AUTO_TEST_SUITE( velocity_verlet_test )

BOOST_FIXTURE_TEST_CASE( test_with_array_ref , velocity_verlet_fixture )
{
    array_stepper stepper;
    array_type q , p ;
    init_state( q , p );
    stepper.do_step( ode() , std::make_pair( boost::ref( q ) , boost::ref( p ) ) , 0.0 , 0.01 );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 2 ) );
}

BOOST_FIXTURE_TEST_CASE( test_with_array_pair , velocity_verlet_fixture )
{
    array_stepper stepper;
    std::pair< array_type , array_type > xxx;
    init_state( xxx.first , xxx.second );
    stepper.do_step( ode() , xxx , 0.0 , 0.01 );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 2 ) );
}

BOOST_FIXTURE_TEST_CASE( test_with_vector_ref , velocity_verlet_fixture )
{
    vector_stepper stepper;
    vector_type q( 2 ) , p( 2 );
    init_state( q , p );
    stepper.do_step( ode() , std::make_pair( boost::ref( q ) , boost::ref( p ) ) , 0.0 , 0.01 );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 2 ) );
}

BOOST_FIXTURE_TEST_CASE( test_with_vector_pair , velocity_verlet_fixture )
{
    vector_stepper stepper;
    std::pair< vector_type , vector_type > x;
    x.first.resize( 2 ) ; x.second.resize( 2 );
    init_state( x.first , x.second );
    stepper.do_step( ode() , x , 0.0 , 0.01 );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 2 ) );
}

BOOST_FIXTURE_TEST_CASE( test_initial_resizer , velocity_verlet_fixture )
{
    typedef get_resizer_test_stepper< initially_resizer >::type stepper_type;
    std::pair< test_array_type , test_array_type > x;
    init_state( x.first , x.second );
    stepper_type stepper;
    stepper.do_step( ode() , x , 0.0 , 0.01 );
    stepper.do_step( ode() , x , 0.0 , 0.01 );
    BOOST_CHECK_EQUAL( adjust_size_count , size_t( 2 ) );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 3 ) );
}

BOOST_FIXTURE_TEST_CASE( test_always_resizer , velocity_verlet_fixture )
{
    typedef get_resizer_test_stepper< always_resizer >::type stepper_type;
    std::pair< test_array_type , test_array_type > x;
    init_state( x.first , x.second );
    stepper_type stepper;
    stepper.do_step( ode() , x , 0.0 , 0.01 );
    stepper.do_step( ode() , x , 0.0 , 0.01 );
    BOOST_CHECK_EQUAL( adjust_size_count , size_t( 4 ) );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 4 ) );    // attention: one more system call, since the size of the state has been changed
}

BOOST_FIXTURE_TEST_CASE( test_with_never_resizer , velocity_verlet_fixture )
{
    typedef get_resizer_test_stepper< never_resizer >::type stepper_type;
    std::pair< test_array_type , test_array_type > x;
    init_state( x.first , x.second );
    stepper_type stepper;
    stepper.do_step( ode() , x , 0.0 , 0.01 );
    stepper.do_step( ode() , x , 0.0 , 0.01 );
    BOOST_CHECK_EQUAL( adjust_size_count , size_t( 0 ) );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 3 ) );
}

BOOST_FIXTURE_TEST_CASE( test_reset , velocity_verlet_fixture )
{
    typedef get_resizer_test_stepper< initially_resizer >::type stepper_type;
    std::pair< test_array_type , test_array_type > x;
    init_state( x.first , x.second );
    stepper_type stepper;
    stepper.do_step( ode() , x , 0.0 , 0.01 );
    BOOST_CHECK_EQUAL( adjust_size_count , size_t( 2 ) );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 2 ) );
    stepper.do_step( ode() , x , 0.0 , 0.01 );
    BOOST_CHECK_EQUAL( adjust_size_count , size_t( 2 ) );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 3 ) );
    stepper.reset();
    BOOST_CHECK_EQUAL( adjust_size_count , size_t( 2 ) );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 3 ) );
    stepper.do_step( ode() , x , 0.0 , 0.01 );
    BOOST_CHECK_EQUAL( adjust_size_count , size_t( 2 ) );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 5 ) );    
}

BOOST_FIXTURE_TEST_CASE( test_initialize1 , velocity_verlet_fixture )
{
    typedef get_resizer_test_stepper< initially_resizer >::type stepper_type;
    std::pair< test_array_type , test_array_type > x;
    init_state( x.first , x.second );
    stepper_type stepper;
    test_array_type ain;
    ode()( x.first , x.second , ain , 0.0 );
    BOOST_CHECK_EQUAL( adjust_size_count , size_t( 0 ) );
    stepper.initialize( ain );
    BOOST_CHECK_EQUAL( adjust_size_count , size_t( 2 ) );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 1 ) );
    stepper.do_step( ode() , x , 0.0 , 0.01 );
    BOOST_CHECK_EQUAL( adjust_size_count , size_t( 2 ) );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 2 ) );
}

BOOST_FIXTURE_TEST_CASE( test_initialize2 , velocity_verlet_fixture )
{
    typedef get_resizer_test_stepper< initially_resizer >::type stepper_type;
    std::pair< test_array_type , test_array_type > x;
    init_state( x.first , x.second );
    stepper_type stepper;
    stepper.initialize( ode() , x.first , x.second , 0.0 );
    BOOST_CHECK_EQUAL( adjust_size_count , size_t( 2 ) );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 1 ) );
    stepper.do_step( ode() , x , 0.0 , 0.01 );
    BOOST_CHECK_EQUAL( adjust_size_count , size_t( 2 ) );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 2 ) );
}


BOOST_FIXTURE_TEST_CASE( test_adjust_size , velocity_verlet_fixture )
{
    typedef get_resizer_test_stepper< initially_resizer >::type stepper_type;
    std::pair< test_array_type , test_array_type > x;
    init_state( x.first , x.second );
    stepper_type stepper;
    stepper.do_step( ode() , x , 0.0 , 0.01 );
    BOOST_CHECK_EQUAL( adjust_size_count , size_t( 2 ) );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 2 ) );
    stepper.adjust_size( x.first );
    BOOST_CHECK_EQUAL( adjust_size_count , size_t( 4 ) );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 2 ) );    
    stepper.do_step( ode() , x , 0.0 , 0.01 );    
    BOOST_CHECK_EQUAL( adjust_size_count , size_t( 4 ) );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 4 ) );
}

BOOST_FIXTURE_TEST_CASE( test_with_unit_pair , velocity_verlet_fixture )
{
    typedef velocity_verlet< coor_vector , velocity_vector , value_type , accelartion_vector ,
        time_type , time_2_type , fusion_algebra , default_operations > stepper_type;
    
    std::pair< coor_vector , velocity_vector > x;
    fusion::at_c< 0 >( x.first ) = 1.0 * si::meter;
    fusion::at_c< 1 >( x.first ) = 0.5 * si::meter;
    fusion::at_c< 0 >( x.second ) = 2.0 * si::meter_per_second;
    fusion::at_c< 1 >( x.second ) = -1.0 * si::meter_per_second;
    stepper_type stepper;
    stepper.do_step( ode_units() , x , 0.0 * si::second , 0.01 * si::second );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 2 ) );
}


BOOST_FIXTURE_TEST_CASE( test_with_unit_ref , velocity_verlet_fixture )
{
    typedef velocity_verlet< coor_vector , velocity_vector , value_type , accelartion_vector ,
        time_type , time_2_type , fusion_algebra , default_operations > stepper_type;
    
    coor_vector q;
    velocity_vector p;
    fusion::at_c< 0 >( q ) = 1.0 * si::meter;
    fusion::at_c< 1 >( q ) = 0.5 * si::meter;
    fusion::at_c< 0 >( p ) = 2.0 * si::meter_per_second;
    fusion::at_c< 1 >( p ) = -1.0 * si::meter_per_second;
    stepper_type stepper;
    stepper.do_step( ode_units() , std::make_pair( boost::ref( q ) , boost::ref( p ) ) , 0.0 * si::second , 0.01 * si::second );
    BOOST_CHECK_EQUAL( ode_call_count , size_t( 2 ) );
}


BOOST_AUTO_TEST_SUITE_END()
