/* test_beta.cpp
 *
 * Copyright Steven Watanabe 2014
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * $Id$
 *
 */

#include <boost/random/beta_distribution.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/math/distributions/beta.hpp>

#define BOOST_RANDOM_DISTRIBUTION boost::random::beta_distribution<>
#define BOOST_RANDOM_DISTRIBUTION_NAME beta
#define BOOST_MATH_DISTRIBUTION boost::math::beta_distribution<>
#define BOOST_RANDOM_ARG1_TYPE double
#define BOOST_RANDOM_ARG1_NAME alpha
#define BOOST_RANDOM_ARG1_DEFAULT 1000.0
#define BOOST_RANDOM_ARG1_DISTRIBUTION(n) boost::uniform_real<>(0.00001, n)
#define BOOST_RANDOM_ARG2_TYPE double
#define BOOST_RANDOM_ARG2_NAME beta
#define BOOST_RANDOM_ARG2_DEFAULT 1000.0
#define BOOST_RANDOM_ARG2_DISTRIBUTION(n) boost::uniform_real<>(0.00001, n)

#include "test_real_distribution.ipp"
