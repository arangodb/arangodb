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

#include <boost/test/unit_test.hpp>

#include <boost/mpl/vector.hpp>
#include <boost/range/algorithm/find_if.hpp>

#include <boost/numeric/odeint.hpp>

using namespace boost::unit_test;
using namespace boost::numeric::odeint;
namespace mpl = boost::mpl;

typedef std::vector<double> state_type;

void rhs( const state_type &x , state_type &dxdt , const double t )
{
}


template<class Stepper>
struct perform_test
{
    void operator()( void )
    {
        bulirsch_stoer< state_type > stepper( 1e-9, 0.0, 0.0, 0.0 );
        state_type x( 3, 10.0 );

        print( boost::find_if(
            make_adaptive_time_range( stepper, rhs, x, 0.0, 1.0, 0.01 ),
            pred ) );

    }

    static bool pred( const std::pair< const state_type &, double > &x )
    {
        return ( x.first[0] < 0.0 );
    }

    template<class Iterator>
    void print( Iterator iter )
    {
        std::cout << iter->second << "\t" << iter->first[0] << "\t"
                  << iter->first[1] << "\t" << iter->first[2] << "\n";
    }
};

typedef mpl::vector<
    euler< state_type > ,
    runge_kutta4< state_type > ,
    runge_kutta_cash_karp54< state_type > ,
    runge_kutta_dopri5< state_type > ,
    runge_kutta_fehlberg78< state_type > ,
    bulirsch_stoer< state_type >
    > steppers;


BOOST_AUTO_TEST_SUITE( regression_147_test )

BOOST_AUTO_TEST_CASE_TEMPLATE( regression_147_test , Stepper, 
                               steppers )
{
    perform_test< Stepper > tester;
    tester();
}

BOOST_AUTO_TEST_SUITE_END()
