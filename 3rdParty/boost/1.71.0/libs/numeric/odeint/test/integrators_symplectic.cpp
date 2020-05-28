/*
 [auto_generated]
 libs/numeric/odeint/test/integrators_symplectic.cpp

 [begin_description]
 tba.
 [end_description]

 Copyright 2009-2013 Karsten Ahnert
 Copyright 2009-2013 Mario Mulansky

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/config.hpp>
#ifdef BOOST_MSVC
    #pragma warning(disable:4996)
#endif

#define BOOST_TEST_MODULE odeint_iterators_symplectic

#define BOOST_FUSION_INVOKE_MAX_ARITY 15
#define BOOST_RESULT_OF_NUM_ARGS 15
#define FUSION_MAX_VECTOR_SIZE 15

#include <cmath>

#include <boost/test/unit_test.hpp>

#ifndef ODEINT_INTEGRATE_ITERATOR
#include <boost/numeric/odeint/integrate/integrate_const.hpp>
#else
#include <boost/numeric/odeint/iterator/integrate/integrate_const.hpp>
#endif
#include <boost/numeric/odeint/stepper/symplectic_euler.hpp>
#include <boost/numeric/odeint/stepper/symplectic_rkn_sb3a_mclachlan.hpp>
#include <boost/numeric/odeint/stepper/symplectic_rkn_sb3a_m4_mclachlan.hpp>

using namespace boost::unit_test;
using namespace boost::numeric::odeint;
namespace mpl = boost::mpl;
namespace fusion = boost::fusion;

typedef std::vector<double> container_type;

struct harm_osc {
    void operator()( const container_type &q , container_type &dpdt )
    {
        dpdt[0] = -q[0];
    }
};

class complete_steppers : public mpl::vector<
    symplectic_euler< container_type >
    , symplectic_rkn_sb3a_mclachlan< container_type >
    , symplectic_rkn_sb3a_m4_mclachlan< container_type >
    > {};

BOOST_AUTO_TEST_SUITE( symplectic_steppers_test )


BOOST_AUTO_TEST_CASE_TEMPLATE( test_integrate_const , Stepper , complete_steppers )
{
    container_type q( 1 , 1.0 ) , p( 1 , 0.0 );
    size_t steps = integrate_const( Stepper() , harm_osc() ,
                                    std::make_pair( boost::ref(q) , boost::ref(p) ) ,
                                    0.0 , 1.0 , 0.1 );
    BOOST_CHECK( steps == 10 );
    BOOST_CHECK_CLOSE( q[0] , std::cos(1.0) , 100*std::pow( 0.1 , Stepper::order_value ) );
}

BOOST_AUTO_TEST_SUITE_END()
