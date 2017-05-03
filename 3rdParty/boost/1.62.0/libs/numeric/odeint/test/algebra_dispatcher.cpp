/*
 [auto_generated]
 libs/numeric/odeint/test/algebra_dispatcher.cpp

 [begin_description]
 tba.
 [end_description]

 Copyright 2013 Karsten Ahnert
 Copyright 2013 Mario Mulansky
 Copyright 2013 Pascal Germroth

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/config.hpp>
#ifdef BOOST_MSVC
    #pragma warning(disable:4996)
#endif

#define BOOST_TEST_MODULE odeint_algebra_dispatcher

#include <boost/numeric/odeint/config.hpp>
#include <boost/numeric/odeint/algebra/algebra_dispatcher.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta4.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta_fehlberg78.hpp>
#include <boost/numeric/odeint/algebra/fusion_algebra_dispatcher.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/array.hpp>
#include <boost/mpl/list.hpp>

using namespace boost::unit_test;
using namespace boost::numeric::odeint;


BOOST_AUTO_TEST_SUITE( algebra_dispatcher_test )

BOOST_AUTO_TEST_CASE( range_algebra_with_vector )
{
    typedef runge_kutta4< std::vector< double > > stepper_type;
    BOOST_STATIC_ASSERT(( boost::is_same< stepper_type::algebra_type , range_algebra >::value ));
}

BOOST_AUTO_TEST_CASE( array_algebra_with_array )
{
    typedef runge_kutta4< boost::array< double , 2 > > stepper_type;
    BOOST_STATIC_ASSERT(( boost::is_same< stepper_type::algebra_type , array_algebra >::value ));
}

BOOST_AUTO_TEST_CASE( range_algebra_with_array )
{
    typedef runge_kutta4< boost::array< double , 2 > , double , boost::array< double , 2 > , double , range_algebra > stepper_type;
    BOOST_STATIC_ASSERT(( boost::is_same< stepper_type::algebra_type , range_algebra >::value ));
}

BOOST_AUTO_TEST_CASE( fusion_algebra_with_fusion_vector )
{
    typedef runge_kutta4< boost::fusion::vector< double > > stepper_type;
    BOOST_STATIC_ASSERT(( boost::is_same< stepper_type::algebra_type , fusion_algebra >::value ));
}

BOOST_AUTO_TEST_CASE( fusion_algebra_with_fusion_vector2 )
{
    typedef runge_kutta_fehlberg78< boost::fusion::vector< double > > stepper_type;
    BOOST_STATIC_ASSERT(( boost::is_same< stepper_type::algebra_type , fusion_algebra >::value ));
}

typedef boost::mpl::list< float , double , long double , std::complex< double > , std::complex< float > > fp_types;
BOOST_AUTO_TEST_CASE_TEMPLATE( vector_space_algebra_with_floating_point , T , fp_types )
{
    typedef runge_kutta_fehlberg78< T > stepper_type;
    BOOST_STATIC_ASSERT(( boost::is_same< typename stepper_type::algebra_type , vector_space_algebra >::value ));
}



BOOST_AUTO_TEST_SUITE_END()
