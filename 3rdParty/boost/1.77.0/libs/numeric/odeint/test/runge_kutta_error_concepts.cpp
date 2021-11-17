/*
 [auto_generated]
 libs/numeric/odeint/test/runge_kutta_error_concepts.cpp

 [begin_description]
 This file tests the Stepper concepts of odeint with all Runge-Kutta Error steppers. 
 It's one of the main tests of odeint.
 [end_description]

 Copyright 2012 Karsten Ahnert
 Copyright 2012-2013 Mario Mulansky

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */

// disable checked iterator warning for msvc
#include <boost/config.hpp>
#ifdef BOOST_MSVC
    #pragma warning(disable:4996)
#endif

#define BOOST_TEST_MODULE odeint_runge_kutta_error_concepts

#include <vector>
#include <cmath>
#include <iostream>

#include <boost/numeric/odeint/config.hpp>

#include <boost/array.hpp>

#include <boost/test/unit_test.hpp>

#include <boost/ref.hpp>
#include <boost/bind.hpp>
#include <boost/utility.hpp>
#include <boost/type_traits/add_reference.hpp>

#include <boost/mpl/vector.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/insert_range.hpp>
#include <boost/mpl/end.hpp>
#include <boost/mpl/copy.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/mpl/inserter.hpp>

#include <boost/numeric/odeint/stepper/runge_kutta_cash_karp54_classic.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta_cash_karp54.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta_dopri5.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta_fehlberg78.hpp>

#include "prepare_stepper_testing.hpp"
#include "dummy_odes.hpp"

using std::vector;

using namespace boost::unit_test;
using namespace boost::numeric::odeint;
namespace mpl = boost::mpl;

const double result = 2.4; // four steps total...

const double eps = 1.0e-14;

template< class Stepper , class System >
void check_error_stepper_concept( Stepper &stepper , System system , typename Stepper::state_type &x , typename Stepper::state_type &xerr )
{
    typedef Stepper stepper_type;
    typedef typename stepper_type::deriv_type container_type;
    typedef typename stepper_type::order_type order_type;
    typedef typename stepper_type::time_type time_type;

    stepper.do_step( system , x , static_cast<time_type>(0.0) , static_cast<time_type>(0.1) );
    stepper.do_step( system , x , static_cast<time_type>(0.0) , static_cast<time_type>(0.1) , xerr );
}

// default case is used for vector space types like plain double
template< class Stepper , typename T >
struct perform_error_stepper_test
{
    typedef T vector_space_type;
    void operator()( void ) const
    {
        using std::abs;
        vector_space_type x , xerr;
        x = 2.0;
        Stepper stepper;
        constant_system_functor_vector_space sys;
#ifndef _MSC_VER
        // dont run this for MSVC due to compiler bug 697006
        check_error_stepper_concept( stepper ,
                                     constant_system_vector_space< vector_space_type , vector_space_type , typename Stepper::time_type > ,
                                     x ,
                                     xerr );
#else
        check_error_stepper_concept( stepper , boost::cref( sys ) , x , xerr );
#endif
        check_error_stepper_concept( stepper , boost::cref( sys ) , x , xerr );

        BOOST_CHECK_MESSAGE( (abs( x - result )) < eps , x );
    }
};

template< class Stepper , typename T >
struct perform_error_stepper_test< Stepper , std::vector<T> >
{
    typedef std::vector<T> vector_type;
    void operator()( void )
    {
        using std::abs;
        vector_type x( 1 , 2.0 ) , xerr( 1 );
        Stepper stepper;
        constant_system_functor_standard sys;
#ifndef _MSC_VER
        // dont run this for MSVC due to compiler bug 697006
        check_error_stepper_concept( stepper , constant_system_standard< vector_type , vector_type , typename Stepper::time_type > , x , xerr );
#else
        check_error_stepper_concept( stepper , boost::cref( sys ) , x , xerr );
#endif
        check_error_stepper_concept( stepper , boost::cref( sys ) , x , xerr );
        BOOST_CHECK( (abs( x[0] - result )) < eps );
    }
};


template< class Stepper , typename T >
struct perform_error_stepper_test< Stepper , boost::array<T,1> >
{
    typedef boost::array<T,1> array_type;
    void operator()( void )
    {
        using std::abs;
        array_type x , xerr;
        x[0] = 2.0;
        Stepper stepper;
        constant_system_functor_standard sys;
#ifndef _MSC_VER
        // dont run this for MSVC due to compiler bug 697006
        check_error_stepper_concept( stepper , constant_system_standard< array_type , array_type , typename Stepper::time_type > , x , xerr );
#else
        check_error_stepper_concept( stepper , boost::cref( sys ) , x , xerr );
#endif
        check_error_stepper_concept( stepper , boost::cref( sys ) , x , xerr );
        BOOST_CHECK( (abs( x[0] - result )) < eps );
    }
};

template< class State > class error_stepper_methods : public mpl::vector<
    runge_kutta_cash_karp54_classic< State , typename detail::extract_value_type<State>::type > ,
    runge_kutta_cash_karp54< State , typename detail::extract_value_type<State>::type > ,
    runge_kutta_dopri5< State , typename detail::extract_value_type<State>::type > ,
    runge_kutta_fehlberg78< State , typename detail::extract_value_type<State>::type >
    > { };


typedef mpl::copy
<
    container_types ,
    mpl::inserter
    <
        mpl::vector0<> ,
        mpl::insert_range
        <
            mpl::_1 ,
            mpl::end< mpl::_1 > ,
            error_stepper_methods< mpl::_2 >
            >
        >
    >::type all_error_stepper_methods;


BOOST_AUTO_TEST_SUITE( runge_kutta_error_concept_test )

BOOST_AUTO_TEST_CASE_TEMPLATE( error_stepper_test , Stepper , all_error_stepper_methods )
{
    perform_error_stepper_test< Stepper , typename Stepper::state_type > tester;
    tester();
}

BOOST_AUTO_TEST_SUITE_END()
