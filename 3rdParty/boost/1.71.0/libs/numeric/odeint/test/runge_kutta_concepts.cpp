/*
 [auto_generated]
 libs/numeric/odeint/test/runge_kutta_concepts.cpp

 [begin_description]
 This file tests the Stepper concepts of odeint with all Runge-Kutta steppers. It's one of the main tests
 of odeint.
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

#define BOOST_TEST_MODULE odeint_runge_kutta_concepts

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

#include <boost/numeric/odeint/stepper/euler.hpp>
#include <boost/numeric/odeint/stepper/modified_midpoint.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta4_classic.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta4.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta_cash_karp54_classic.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta_cash_karp54.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta_dopri5.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta_fehlberg78.hpp>

#include <boost/numeric/odeint/algebra/detail/extract_value_type.hpp>

#include "prepare_stepper_testing.hpp"
#include "dummy_odes.hpp"

using std::vector;

using namespace boost::unit_test;
using namespace boost::numeric::odeint;
namespace mpl = boost::mpl;

const double result = 2.2;

const double eps = 1.0e-14;

template< class Stepper , class System >
void check_stepper_concept( Stepper &stepper , System system , typename Stepper::state_type &x )
{
    typedef Stepper stepper_type;
    typedef typename stepper_type::deriv_type container_type;
    typedef typename stepper_type::order_type order_type;
    typedef typename stepper_type::time_type time_type;

    stepper.do_step( system , x , static_cast<time_type>(0.0) , static_cast<time_type>(0.1) );
}

// default case is used for vector space types like plain double
template< class Stepper , typename T >
struct perform_stepper_test
{
    typedef T vector_space_type;
    void operator()( void ) const
    {
        using std::abs;
        vector_space_type x;
        x = 2.0;
        Stepper stepper;
        constant_system_functor_vector_space sys;
#ifndef _MSC_VER
        // dont run this for MSVC due to compiler bug 697006
        check_stepper_concept( stepper , constant_system_vector_space< vector_space_type , vector_space_type , typename Stepper::time_type > , x );
#else
        check_stepper_concept( stepper , boost::cref( sys ) , x );
#endif
        check_stepper_concept( stepper , boost::cref( sys ) , x );
        std::cout << x << " ?= " << result << std::endl;
        BOOST_CHECK( (abs( x - result )) < eps );
    }
};

template< class Stepper , typename T >
struct perform_stepper_test< Stepper , std::vector<T> >
{
    typedef std::vector<T> vector_type;
    void operator()( void )
    {
        using std::abs;
        vector_type x( 1 , static_cast<T>(2.0) );
        Stepper stepper;
        constant_system_functor_standard sys;
#ifndef _MSC_VER
        // dont run this for MSVC due to compiler bug 697006
        check_stepper_concept( stepper , constant_system_standard< vector_type , vector_type , typename Stepper::time_type > , x );
#else
        check_stepper_concept( stepper , boost::cref( sys ) , x );
#endif
        check_stepper_concept( stepper , boost::cref( sys ) , x );
        std::cout << x[0] << " ?= " << result << std::endl;
        BOOST_CHECK( (abs( x[0] - result )) < eps );
    }
};

template< class Stepper , typename T >
struct perform_stepper_test< Stepper , boost::array<T,1> >
{
    typedef boost::array<T,1> array_type;
    void operator()( void )
    {
        using std::abs;
        array_type x;
        x[0] = static_cast<T>(2.0);
        Stepper stepper;
        constant_system_functor_standard sys;
#ifndef _MSC_VER
        // dont run this for MSVC due to compiler bug 697006
        check_stepper_concept( stepper , constant_system_standard< array_type , array_type , typename Stepper::time_type > , x );
#else
        check_stepper_concept( stepper , boost::cref( sys ) , x );
#endif
        check_stepper_concept( stepper , boost::cref( sys ) , x );
        std::cout << x[0] << " ?= " << result << std::endl;
        BOOST_CHECK( (abs( x[0] - result )) < eps );
    }
};

// split stepper methods to ensure the final vector has less than 30(?) elements
// (stepper_methods*container_types) < 30(?)
template< class State > class stepper_methods1 : public mpl::vector<
    euler< State , typename detail::extract_value_type<State>::type > ,
    modified_midpoint< State , typename detail::extract_value_type<State>::type > ,
    runge_kutta4< State , typename detail::extract_value_type<State>::type > ,
    runge_kutta4_classic< State , typename detail::extract_value_type<State>::type >
    > { };

template< class State > class stepper_methods2 : public mpl::vector<
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
      stepper_methods1< mpl::_2 >
    >
  >
>::type stepper_combinations1;

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
      stepper_methods2< mpl::_2 >
    >
  >
>::type stepper_combinations2;


BOOST_AUTO_TEST_SUITE( runge_kutta_concept_test )

BOOST_AUTO_TEST_CASE_TEMPLATE( stepper_test1 , Stepper, stepper_combinations1 )
{
    perform_stepper_test< Stepper , typename Stepper::deriv_type > tester;
    tester();
}

BOOST_AUTO_TEST_CASE_TEMPLATE( stepper_test2 , Stepper, stepper_combinations2 )
{
    perform_stepper_test< Stepper , typename Stepper::deriv_type > tester;
    tester();
}


BOOST_AUTO_TEST_SUITE_END()
