/* test_uniform_on_sphere.cpp
 *
 * Copyright Steven Watanabe 2011
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * $Id$
 *
 */

#include <boost/random/uniform_on_sphere.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/math/distributions/uniform.hpp>
#include <cmath>

class uniform_on_sphere_test {
public:
    typedef double result_type;
    uniform_on_sphere_test(int dims, int x, int y)
        : impl(dims), idx1(x), idx2(y) {}
    template<class Engine>
    result_type operator()(Engine& rng) {
        const boost::random::uniform_on_sphere<>::result_type& tmp = impl(rng);
        // This should be uniformly distributed in [-pi,pi)
        return std::atan2(tmp[idx1], tmp[idx2]);
    }
private:
    boost::random::uniform_on_sphere<> impl;
    int idx1, idx2;
};

static const double pi = 3.14159265358979323846;

#define BOOST_RANDOM_DISTRIBUTION uniform_on_sphere_test
#define BOOST_RANDOM_DISTRIBUTION_NAME uniform_on_sphere
#define BOOST_MATH_DISTRIBUTION boost::math::uniform
#define BOOST_RANDOM_ARG1_TYPE double
#define BOOST_RANDOM_ARG1_NAME n
#define BOOST_RANDOM_ARG1_DEFAULT 6
#define BOOST_RANDOM_ARG1_DISTRIBUTION(n) boost::uniform_int<>(2, n)
#define BOOST_RANDOM_DISTRIBUTION_INIT (n, 0, n-1)
#define BOOST_MATH_DISTRIBUTION_INIT (-pi, pi)

#include "test_real_distribution.ipp"
