/*
 [auto_generated]
 libs/numeric/odeint/test/n_step_time_iterator.cpp

 [begin_description]
 This file tests the n-step time iterator.
 [end_description]

 Copyright 2009-2013 Karsten Ahnert
 Copyright 2009-2013 Mario Mulansky

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */


#define BOOST_TEST_MODULE odeint_n_step_time_iterator

#include <iterator>
#include <algorithm>
#include <vector>

#include <boost/numeric/odeint/config.hpp>
#include <boost/array.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/mpl/vector.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>

#include <boost/numeric/odeint/iterator/n_step_time_iterator.hpp>
#include "dummy_steppers.hpp"
#include "dummy_odes.hpp"
#include "dummy_observers.hpp"

namespace mpl = boost::mpl;
using namespace boost::numeric::odeint;

typedef dummy_stepper::state_type state_type;
typedef dummy_stepper::value_type value_type;
typedef dummy_stepper::time_type time_type;
typedef std::vector< std::pair< state_type , time_type > > result_vector;

BOOST_AUTO_TEST_SUITE( n_step_time_iterator_test )

typedef mpl::vector<
    dummy_stepper
    , dummy_dense_output_stepper
    > dummy_steppers;



BOOST_AUTO_TEST_CASE_TEMPLATE( copy_stepper_iterator , Stepper , dummy_steppers )
{
    typedef n_step_time_iterator< Stepper , empty_system , state_type > iterator_type;
    state_type x = {{ 1.0 }};
    iterator_type iter1 = iterator_type( Stepper() , empty_system() , x , 0.0 , 0.1 , 10 );
    iterator_type iter2 = iter1;
    BOOST_CHECK_EQUAL( &( iter1->first ) , &( iter2->first ) );
    BOOST_CHECK_EQUAL( &( iter1->first ) , &x );
    BOOST_CHECK( iter1.same( iter2 ) );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( assignment_stepper_iterator , Stepper , dummy_steppers )
{
    typedef n_step_time_iterator< Stepper , empty_system , state_type > iterator_type;
    state_type x1 = {{ 1.0 }} , x2 = {{ 2.0 }};
    iterator_type iter1 = iterator_type( Stepper() , empty_system() , x1 , 0.0 , 0.1 , 10 );
    iterator_type iter2 = iterator_type( Stepper() , empty_system() , x2 , 0.0 , 0.2 , 10 );
    BOOST_CHECK_EQUAL( &( iter1->first ) , &x1 );
    BOOST_CHECK_EQUAL( &( iter2->first ) , &x2 );
    BOOST_CHECK( !iter1.same( iter2 ) );
    iter2 = iter1;
    BOOST_CHECK_EQUAL( &( iter1->first ) , &x1 );
    BOOST_CHECK_EQUAL( &( iter2->first ) , &x1 );
    BOOST_CHECK( iter1.same( iter2 ) );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( stepper_iterator_factory , Stepper , dummy_steppers )
{
    Stepper stepper;
    empty_system system;
    state_type x = {{ 1.0 }};

    std::for_each(
        make_n_step_time_iterator_begin( stepper , boost::ref( system ) , x , 0.0 , 0.1 , 10 ) ,
        make_n_step_time_iterator_end( stepper , boost::ref( system ) , x ) ,
        dummy_observer() );

    BOOST_CHECK_CLOSE( x[0] , 3.5 , 1.0e-13 );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( stepper_range , Stepper , dummy_steppers )
{
    Stepper stepper;
    empty_system system;
    state_type x = {{ 1.0 }};

    boost::for_each( make_n_step_time_range( stepper , boost::ref( system ) , x , 0.0 , 0.1 , 10 ) ,
                     dummy_observer() );

    BOOST_CHECK_CLOSE( x[0] , 3.5 , 1.0e-13 );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( stepper_iterator_with_reference_wrapper_factory , Stepper , dummy_steppers )
{
    Stepper stepper;
    empty_system system;
    state_type x = {{ 1.0 }};

    std::for_each(
        make_n_step_time_iterator_begin( boost::ref( stepper ) , boost::ref( system ) , x , 0.0 , 0.1 , 10 ) ,
        make_n_step_time_iterator_end( boost::ref( stepper ) , boost::ref( system ) , x ) ,
        dummy_observer() );

    BOOST_CHECK_CLOSE( x[0] , 3.5 , 1.0e-13 );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( stepper_range_with_reference_wrapper , Stepper , dummy_steppers )
{
    Stepper stepper;
    empty_system system;
    state_type x = {{ 1.0 }};

    boost::for_each( make_n_step_time_range( boost::ref( stepper ) , boost::ref( system ) , x , 0.0 , 0.1 , 10 ) ,
                     dummy_observer() );

    BOOST_CHECK_CLOSE( x[0] , 3.5 , 1.0e-13 );
}



BOOST_AUTO_TEST_CASE_TEMPLATE( transitivity1 , Stepper , dummy_steppers )
{
    typedef n_step_time_iterator< Stepper , empty_system , state_type > stepper_iterator;

    state_type x = {{ 1.0 }};
    stepper_iterator first1( Stepper() , empty_system() , x , 0.0 , 0.1 , 0 );
    stepper_iterator last1( Stepper() , empty_system() , x );
    stepper_iterator last2( Stepper() , empty_system() , x );

    BOOST_CHECK( last1 == last2 );
    BOOST_CHECK( first1 != last1 );
    BOOST_CHECK( ++first1 == last1 );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( copy_algorithm , Stepper , dummy_steppers )
{
    typedef n_step_time_iterator< Stepper , empty_system , state_type > stepper_iterator;
    state_type x = {{ 1.0 }};
    result_vector res;
    stepper_iterator first( Stepper() , empty_system() , x , 0.0 , 0.1 , 3 );
    stepper_iterator last( Stepper() , empty_system() , x );

    std::copy( first , last , std::back_insert_iterator< result_vector >( res ) );

    BOOST_CHECK_EQUAL( res.size() , size_t( 4 ) );
    BOOST_CHECK_CLOSE( res[0].first[0] , 1.0 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[0].second , 0.0 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[1].first[0] , 1.25 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[1].second , 0.1 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[2].first[0] , 1.5 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[2].second , 0.2 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[3].first[0] , 1.75 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[3].second , 0.3 , 1.0e-13 );
    BOOST_CHECK_CLOSE( x[0] , 1.75 , 1.0e-13 );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( copy_algorithm_with_factory , Stepper , dummy_steppers )
{
    state_type x = {{ 1.0 }};
    result_vector res;
    std::copy( make_n_step_time_iterator_begin( Stepper() , empty_system() , x , 0.0 , 0.1 , 3 ) ,
               make_n_step_time_iterator_end( Stepper() , empty_system() , x ) ,
               std::back_insert_iterator< result_vector >( res ) );

    BOOST_CHECK_EQUAL( res.size() , size_t( 4 ) );
    BOOST_CHECK_CLOSE( res[0].first[0] , 1.0 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[0].second , 0.0 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[1].first[0] , 1.25 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[1].second , 0.1 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[2].first[0] , 1.5 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[2].second , 0.2 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[3].first[0] , 1.75 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[3].second , 0.3 , 1.0e-13 );
    BOOST_CHECK_CLOSE( x[0] , 1.75 , 1.0e-13 );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( copy_algorithm_with_range_factory , Stepper , dummy_steppers )
{
    state_type x = {{ 1.0 }};
    result_vector res;
    boost::range::copy( make_n_step_time_range( Stepper() , empty_system() , x , 0.0 , 0.1 , 3 ) ,
                        std::back_insert_iterator< result_vector >( res ) );

    BOOST_CHECK_EQUAL( res.size() , size_t( 4 ) );
    BOOST_CHECK_CLOSE( res[0].first[0] , 1.0 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[0].second , 0.0 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[1].first[0] , 1.25 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[1].second , 0.1 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[2].first[0] , 1.5 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[2].second , 0.2 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[3].first[0] , 1.75 , 1.0e-13 );
    BOOST_CHECK_CLOSE( res[3].second , 0.3 , 1.0e-13 );
    BOOST_CHECK_CLOSE( x[0] , 1.75 , 1.0e-13 );
}




BOOST_AUTO_TEST_SUITE_END()
