/*
 [auto_generated]
 libs/numeric/odeint/test/resizing.cpp

 [begin_description]
 This file tests the resizing mechanism of odeint.
 [end_description]

 Copyright 2010-2012 Karsten Ahnert
 Copyright 2010-2012 Mario Mulansky

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */

// disable checked iterator warning for msvc
#include <boost/config.hpp>
#ifdef BOOST_MSVC
    #pragma warning(disable:4996)
#endif

#define BOOST_TEST_MODULE odeint_resize

#include <vector>
#include <cmath>

#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/utility.hpp>
#include <boost/type_traits/integral_constant.hpp>

#include <boost/test/unit_test.hpp>

#include <boost/mpl/vector.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/at.hpp>

#include <boost/numeric/odeint/stepper/euler.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta4_classic.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta4.hpp>
#include <boost/numeric/odeint/algebra/vector_space_algebra.hpp>

#include <boost/numeric/odeint/util/resizer.hpp>
#include <boost/numeric/odeint/util/is_resizeable.hpp>

#include "resizing_test_state_type.hpp"

using namespace boost::unit_test;
using namespace boost::numeric::odeint;

namespace mpl = boost::mpl;






void constant_system( const test_array_type &x , test_array_type &dxdt , double t ) { dxdt[0] = 1.0; }


BOOST_AUTO_TEST_SUITE( check_resize_test )


typedef euler< test_array_type , double , test_array_type , double , range_algebra , default_operations , never_resizer > euler_manual_type;
typedef euler< test_array_type , double , test_array_type , double , range_algebra , default_operations , initially_resizer > euler_initially_type;
typedef euler< test_array_type , double , test_array_type , double , range_algebra , default_operations , always_resizer > euler_always_type;

typedef runge_kutta4_classic< test_array_type , double , test_array_type , double , range_algebra , default_operations , never_resizer > rk4_manual_type;
typedef runge_kutta4_classic< test_array_type , double , test_array_type , double , range_algebra , default_operations , initially_resizer > rk4_initially_type;
typedef runge_kutta4_classic< test_array_type , double , test_array_type , double , range_algebra , default_operations , always_resizer > rk4_always_type;


typedef runge_kutta4< test_array_type , double , test_array_type , double , range_algebra , default_operations , never_resizer > rk4_gen_manual_type;
typedef runge_kutta4< test_array_type , double , test_array_type , double , range_algebra , default_operations , initially_resizer > rk4_gen_initially_type;
typedef runge_kutta4< test_array_type , double , test_array_type , double , range_algebra , default_operations , always_resizer > rk4_gen_always_type;


typedef mpl::vector<
    mpl::vector< euler_manual_type , mpl::int_<1> , mpl::int_<0> > ,
    mpl::vector< euler_initially_type , mpl::int_<1> , mpl::int_<1> > ,
    mpl::vector< euler_always_type , mpl::int_<1> , mpl::int_<3> > ,
    mpl::vector< rk4_manual_type , mpl::int_<5> , mpl::int_<0> > ,
    mpl::vector< rk4_initially_type , mpl::int_<5> , mpl::int_<1> > ,
    mpl::vector< rk4_always_type , mpl::int_<5> , mpl::int_<3> > ,
    mpl::vector< rk4_gen_manual_type , mpl::int_<5> , mpl::int_<0> > ,
    mpl::vector< rk4_gen_initially_type , mpl::int_<5> , mpl::int_<1> > ,
    mpl::vector< rk4_gen_always_type , mpl::int_<5> , mpl::int_<3> >
    >::type resize_check_types;


BOOST_AUTO_TEST_CASE_TEMPLATE( test_resize , T, resize_check_types )
{
    typedef typename mpl::at< T , mpl::int_< 0 > >::type stepper_type;
    const size_t resize_calls = mpl::at< T , mpl::int_< 1 > >::type::value;
    const size_t multiplicity = mpl::at< T , mpl::int_< 2 > >::type::value;
    adjust_size_count = 0;

    stepper_type stepper;
    test_array_type x;
    stepper.do_step( constant_system , x , 0.0 , 0.1 );
    stepper.do_step( constant_system , x , 0.0 , 0.1 );
    stepper.do_step( constant_system , x , 0.0 , 0.1 );

    BOOST_TEST_MESSAGE( "adjust_size_count : " << adjust_size_count );
    BOOST_CHECK_MESSAGE( adjust_size_count == resize_calls * multiplicity , "adjust_size_count : " << adjust_size_count << " expected: " << resize_calls * multiplicity );
}


BOOST_AUTO_TEST_SUITE_END()
