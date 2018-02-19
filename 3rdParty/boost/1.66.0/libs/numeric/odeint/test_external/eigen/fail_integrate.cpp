/*
 [auto_generated]
 fail_integrate.cpp

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

#define BOOST_TEST_MODULE odeint_dummy

#include <boost/numeric/odeint/integrate/integrate.hpp>
#include <boost/numeric/odeint/external/eigen/eigen.hpp>

#include <boost/test/unit_test.hpp>

#include "dummy_odes.hpp"

using namespace boost::unit_test;
using namespace boost::numeric::odeint;


BOOST_AUTO_TEST_SUITE( eigen_fail_integrate )

BOOST_AUTO_TEST_CASE( test )
{
    typedef Eigen::Matrix< double , 1 , 1 > state_type;
    state_type x;
    x[0] = 10.0;
    double t_start = 0.0 , t_end = 1.0 , dt = 0.1;
    integrate( constant_system_functor_standard() , x , t_start , t_end , dt );
}

BOOST_AUTO_TEST_SUITE_END()

