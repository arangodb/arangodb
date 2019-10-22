// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/io.hpp>

#include "utils.hpp"

using namespace boost::numeric::ublas;

static const double TOL(1.0e-5); ///< Used for comparing two real numbers.

BOOST_UBLAS_TEST_DEF ( test_double_scaled_norm_2 ) {
    vector<double> v(2);
    v[0] = 0; v[1] = 1.0e155;

    const double expected = 1.0e155;

    BOOST_UBLAS_DEBUG_TRACE( "norm is " << norm_2(v) );
    BOOST_UBLAS_TEST_CHECK(std::abs(norm_2(v) - expected) < TOL);
}

BOOST_UBLAS_TEST_DEF ( test_float_scaled_norm_2 ) {
    vector<float> v(2);
    v[0] = 0; v[1] = 1.0e20;

    const float expected = 1.0e20;

    BOOST_UBLAS_DEBUG_TRACE( "norm is " << norm_2(v) );
    BOOST_UBLAS_TEST_CHECK(std::abs(norm_2(v) - expected) < TOL);
}

int main() {
    BOOST_UBLAS_TEST_BEGIN();

    BOOST_UBLAS_TEST_DO( test_double_scaled_norm_2 );
    BOOST_UBLAS_TEST_DO( test_float_scaled_norm_2 );

    BOOST_UBLAS_TEST_END();
}
