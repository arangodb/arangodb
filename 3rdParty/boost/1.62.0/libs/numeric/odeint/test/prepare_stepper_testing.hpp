/*
 [auto_generated]
 libs/numeric/odeint/test/prepare_stepper_testing.hpp

 [begin_description]
 This file defines some helper functions for the stepper tests.
 [end_description]

 Copyright 2011-2012 Karsten Ahnert
 Copyright 2012-2013 Mario Mulansky

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#ifndef PREPARE_STEPPER_TESTING_HPP_
#define PREPARE_STEPPER_TESTING_HPP_

#include <boost/array.hpp>
#include <vector>
#include <complex>

#include <boost/fusion/sequence.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

#include <boost/numeric/odeint/algebra/vector_space_algebra.hpp>
#include <boost/numeric/odeint/algebra/array_algebra.hpp>
#include <boost/numeric/odeint/algebra/algebra_dispatcher.hpp>

namespace mpl = boost::mpl;
namespace fusion = boost::fusion;

using namespace boost::numeric::odeint;

/* the state types that will be tested */
typedef std::vector< double > vector_type;
typedef std::vector< std::complex<double> > complex_vector_type;
typedef double vector_space_type;
typedef boost::array< double , 1 > array_type;
typedef boost::array< std::complex<double> , 1 > complex_array_type;

typedef boost::multiprecision::cpp_dec_float_50 mp_type;
typedef boost::array< mp_type , 1 > mp_array_type;

typedef mpl::vector< vector_type , complex_vector_type , vector_space_type ,
                     array_type , complex_array_type , mp_type , mp_array_type
                     >::type container_types;

namespace boost { namespace numeric { namespace odeint {

    // mp_type forms a vector space
    template<>
    struct algebra_dispatcher< mp_type >
    {
        typedef vector_space_algebra algebra_type;
    };

    // add norm computation
    template<>
    struct vector_space_norm_inf< mp_type >
    {
        typedef mp_type result_type;
        mp_type operator()( mp_type x ) const
        {
            using std::abs;
            return abs(x);
        }
    };

} } }

#endif /* PREPARE_STEPPER_TESTING_HPP_ */
