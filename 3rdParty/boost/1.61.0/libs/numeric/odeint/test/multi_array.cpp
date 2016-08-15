/*
 [auto_generated]
 libs/numeric/odeint/test/multi_array.cpp

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

#define BOOST_TEST_MODULE odeint_multi_array

#include <boost/test/unit_test.hpp>

#include <boost/numeric/odeint/algebra/multi_array_algebra.hpp>
#include <boost/numeric/odeint/util/multi_array_adaption.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta4.hpp>
#include <boost/numeric/odeint/integrate/integrate_const.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta_dopri5.hpp>
#include <boost/numeric/odeint/stepper/generation.hpp>

using namespace boost::unit_test;
using namespace boost::numeric::odeint;

typedef boost::multi_array< double , 1 > vector_type;
typedef boost::multi_array< double , 2 > matrix_type;
typedef boost::multi_array< double , 3 > tensor_type;


BOOST_AUTO_TEST_SUITE( multi_array_test )

BOOST_AUTO_TEST_CASE( test_resizeable )
{
    BOOST_CHECK( is_resizeable< vector_type >::value );
    BOOST_CHECK( is_resizeable< matrix_type >::value );
    BOOST_CHECK( is_resizeable< tensor_type >::value );
}

BOOST_AUTO_TEST_CASE( test_same_size_vector1 )
{
    vector_type v1( boost::extents[12] );
    vector_type v2( boost::extents[12] );
    BOOST_CHECK( same_size( v1 , v2 ) );
}

BOOST_AUTO_TEST_CASE( test_same_size_vector2 )
{
    vector_type v1( boost::extents[12] );
    vector_type v2( boost::extents[13] );
    BOOST_CHECK( !same_size( v1 , v2 ) );
}

BOOST_AUTO_TEST_CASE( test_same_size_vector3 )
{
    vector_type v1( boost::extents[12] );
    vector_type v2( boost::extents[vector_type::extent_range(-1,11)] );
    BOOST_CHECK( !same_size( v1 , v2 ) );
}

BOOST_AUTO_TEST_CASE( test_same_size_vector4 )
{
    vector_type v1( boost::extents[12] );
    vector_type v2( boost::extents[vector_type::extent_range(-1,10)] );
    BOOST_CHECK( !same_size( v1 , v2 ) );
}

BOOST_AUTO_TEST_CASE( test_same_size_vector5 )
{
    vector_type v1( boost::extents[vector_type::extent_range(-1,10)] );
    vector_type v2( boost::extents[vector_type::extent_range(-1,10)] );
    BOOST_CHECK( same_size( v1 , v2 ) );
}



BOOST_AUTO_TEST_CASE( test_same_size_matrix1 )
{
    matrix_type m1( boost::extents[12][4] );
    matrix_type m2( boost::extents[12][4] );
    BOOST_CHECK( same_size( m1 , m2 ) );
}

BOOST_AUTO_TEST_CASE( test_same_size_matrix2 )
{
    matrix_type m1( boost::extents[12][4] );
    matrix_type m2( boost::extents[12][3] );
    BOOST_CHECK( !same_size( m1 , m2 ) );
}

BOOST_AUTO_TEST_CASE( test_same_size_matrix3 )
{
    matrix_type m1( boost::extents[12][matrix_type::extent_range(-1,2)] );
    matrix_type m2( boost::extents[12][3] );
    BOOST_CHECK( !same_size( m1 , m2 ) );
}

BOOST_AUTO_TEST_CASE( test_same_size_matrix4 )
{
    matrix_type m1( boost::extents[12][matrix_type::extent_range(-1,1)] );
    matrix_type m2( boost::extents[12][3] );
    BOOST_CHECK( !same_size( m1 , m2 ) );
}

BOOST_AUTO_TEST_CASE( test_same_size_tensor1 )
{
    tensor_type t1( boost::extents[ tensor_type::extent_range( -2 , 9 ) ]
                                  [ tensor_type::extent_range( 5 , 11 ) ]
                                  [ tensor_type::extent_range( -1 , 2 ) ] );
    tensor_type t2( boost::extents[ tensor_type::extent_range( 2 , 13 ) ]
                                  [ tensor_type::extent_range( -1 , 5 ) ]
                                  [ tensor_type::extent_range( 1 , 4 ) ] );
    BOOST_CHECK( !same_size( t1 , t2 ) );    
}

BOOST_AUTO_TEST_CASE( test_same_size_tensor2 )
{
    tensor_type t1( boost::extents[ tensor_type::extent_range( -2 , 9 ) ]
                                  [ tensor_type::extent_range( 5 , 11 ) ]
                                  [ tensor_type::extent_range( -1 , 2 ) ] );
    tensor_type t2( boost::extents[ tensor_type::extent_range( -2 , 9 ) ]
                                  [ tensor_type::extent_range( 5 , 11 ) ]
                                  [ tensor_type::extent_range( -1 , 2 ) ] );
    BOOST_CHECK( same_size( t1 , t2 ) );    
}


// Tests for tensor_type

BOOST_AUTO_TEST_CASE( test_resize_vector1 )
{
    vector_type v1( boost::extents[4] );
    vector_type v2;
    resize( v2 , v1 );
    BOOST_CHECK( same_size( v1 , v2 ) );
}

BOOST_AUTO_TEST_CASE( test_resize_vector2 )
{
    vector_type v1( boost::extents[ vector_type::extent_range( -2 , 9 ) ] );
    vector_type v2;
    resize( v2 , v1 );
    BOOST_CHECK( same_size( v1 , v2 ) );
}

BOOST_AUTO_TEST_CASE( test_resize_vector3 )
{
    vector_type v1( boost::extents[ vector_type::extent_range( -2 , 9 ) ] );
    vector_type v2( boost::extents[ vector_type::extent_range( 2 , 9 ) ] );
    BOOST_CHECK( !same_size( v1 , v2 ) );
    resize( v2 , v1 );
    BOOST_CHECK( same_size( v1 , v2 ) );
}

struct mult4
{
    void operator()( double &a , double const &b ) const
    {
        a = b * 4.0;
    }
};

BOOST_AUTO_TEST_CASE( test_for_each2_vector )
{
    vector_type v1( boost::extents[ vector_type::extent_range( -2 , 9 ) ] );
    vector_type v2( boost::extents[ vector_type::extent_range( 2 , 13 ) ] );
    for( int i=-2 ; i<9 ; ++i )
        v1[i] = i * 2;
    multi_array_algebra::for_each2( v2 , v1 , mult4() );
    for( int i=2 ; i<13 ; ++i )
        BOOST_CHECK_EQUAL( v2[i] , v1[i-4]*4.0 );
}

struct vector_ode
{
    const static size_t n = 128;
    void operator()( const vector_type &x , vector_type &dxdt , double t ) const
    {
        dxdt[-1] = x[n] - 2.0 * x[-1] + x[0];
        for( size_t i=0 ; i<n ; ++i )
            dxdt[i] = x[i-1] - 2.0 * x[i] + x[i+1];
        dxdt[n] = x[-1] - 2.0 * x[n] + x[n-1];
    }
};


BOOST_AUTO_TEST_CASE( test_rk4_vector )
{
    vector_type v1( boost::extents[ vector_type::extent_range( -1 , vector_ode::n+1 ) ] );
    integrate_const( runge_kutta4< vector_type >() , vector_ode() , v1 , 0.0 , 1.0 , 0.01 );
}

BOOST_AUTO_TEST_CASE( test_dopri5_vector )
{
    vector_type v1( boost::extents[ vector_type::extent_range( -1 , vector_ode::n+1 ) ] );
    integrate_const( make_dense_output( 1.0e-6 , 1.0e-6 , runge_kutta_dopri5< vector_type >() ) , vector_ode() , v1 , 0.0 , 1.0 , 0.01 );
}



BOOST_AUTO_TEST_SUITE_END()
