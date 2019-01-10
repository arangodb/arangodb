/*
 
 [begin_description]
 Test case for issue 189: 
 Controlled Rosenbrock stepper fails to increase step size
 [end_description]

 Copyright 2016 Karsten Ahnert
 Copyright 2016 Mario Mulansky

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */
 
#define BOOST_TEST_MODULE odeint_regression_189

#include <boost/numeric/odeint.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>

using namespace boost::numeric::odeint;
namespace phoenix = boost::phoenix;

typedef boost::numeric::ublas::vector< double > vector_type;
typedef boost::numeric::ublas::matrix< double > matrix_type;

struct stiff_system
{
    void operator()( const vector_type &x , vector_type &dxdt , double /* t */ )
    {
        dxdt[ 0 ] = -101.0 * x[ 0 ] - 100.0 * x[ 1 ];
        dxdt[ 1 ] = x[ 0 ];
    }
};

struct stiff_system_jacobi
{
    void operator()( const vector_type & /* x */ , matrix_type &J , const double & /* t */ , vector_type &dfdt )
    {
        J( 0 , 0 ) = -101.0;
        J( 0 , 1 ) = -100.0;
        J( 1 , 0 ) = 1.0;
        J( 1 , 1 ) = 0.0;
        dfdt[0] = 0.0;
        dfdt[1] = 0.0;
    }
};


BOOST_AUTO_TEST_CASE( regression_189 )
{
    vector_type x( 2 , 1.0 );

    size_t num_of_steps = integrate_const( make_dense_output< rosenbrock4< double > >( 1.0e-6 , 1.0e-6 ) ,
            std::make_pair( stiff_system() , stiff_system_jacobi() ) ,
            x , 0.0 , 50.0 , 0.01 ,
            std::cout << phoenix::arg_names::arg2 << " " << phoenix::arg_names::arg1[0] << "\n" );
    // regression: number of steps should be 74
    size_t num_of_steps_expected = 74;
    BOOST_CHECK_EQUAL( num_of_steps , num_of_steps_expected );
    
    vector_type x2( 2 , 1.0 );

    size_t num_of_steps2 = integrate_const( make_dense_output< runge_kutta_dopri5< vector_type > >( 1.0e-6 , 1.0e-6 ) ,
            stiff_system() , x2 , 0.0 , 50.0 , 0.01 ,
            std::cout << phoenix::arg_names::arg2 << " " << phoenix::arg_names::arg1[0] << "\n" );
    num_of_steps_expected = 1531;
    BOOST_CHECK_EQUAL( num_of_steps2 , num_of_steps_expected );
}
