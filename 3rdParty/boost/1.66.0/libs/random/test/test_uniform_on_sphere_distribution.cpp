/* test_uniform_on_sphere_distribution.cpp
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
#include <boost/assign/list_of.hpp>

#include <limits>

#define BOOST_RANDOM_DISTRIBUTION boost::random::uniform_on_sphere<>
#define BOOST_RANDOM_ARG1 dim
#define BOOST_RANDOM_ARG1_DEFAULT 2
#define BOOST_RANDOM_ARG1_VALUE 3

std::vector<double> min0 = boost::assign::list_of(-1.0)(0.0);
std::vector<double> max0 = boost::assign::list_of(1.0)(0.0);
std::vector<double> min1 = boost::assign::list_of(-1.0)(0.0)(0.0);
std::vector<double> max1 = boost::assign::list_of(1.0)(0.0)(0.0);

#define BOOST_RANDOM_DIST0_MIN min0
#define BOOST_RANDOM_DIST0_MAX max0
#define BOOST_RANDOM_DIST1_MIN min1
#define BOOST_RANDOM_DIST1_MAX max1

#define BOOST_RANDOM_TEST1_PARAMS (0)
#define BOOST_RANDOM_TEST1_MIN std::vector<double>()
#define BOOST_RANDOM_TEST1_MAX std::vector<double>()
#define BOOST_RANDOM_TEST2_PARAMS
#define BOOST_RANDOM_TEST2_MIN min0
#define BOOST_RANDOM_TEST2_MAX max0

#include <boost/test/test_tools.hpp>

BOOST_TEST_DONT_PRINT_LOG_VALUE( std::vector<double> )

#include "test_distribution.ipp"

#include <boost/math/special_functions/fpclassify.hpp>

struct generate_zeros {
public:
    generate_zeros() : i(0) {}
    typedef unsigned result_type;
    static unsigned (min)() { return 0u; }
    static unsigned (max)() { return boost::random::minstd_rand0::max(); }
    unsigned operator()() {
        static unsigned data[] = { 0, 0, 0, 0, 0, 0 };
        if(i < 6) {
            return data[i++];
        } else {
            return gen();
        }
    }
private:
    int i;
    boost::random::minstd_rand0 gen;
};

BOOST_AUTO_TEST_CASE(test_zeros) {
    generate_zeros gen;
    boost::random::uniform_on_sphere<> dist(2);
    std::vector<double> val = dist(gen);
    BOOST_CHECK(!(boost::math::isnan)(val[0]));
}

BOOST_AUTO_TEST_CASE(test_valid_output) {
    boost::random::minstd_rand0 gen;
    for(int n = 0; n < 10; ++n) {
        boost::random::uniform_on_sphere<> dist(n);
        std::vector<double> result = dist(gen);
        BOOST_TEST(result.size() == static_cast<std::size_t>(n));
        if(n > 0) {
            double sum_sq = 0;
            for(std::size_t j = 0; j < result.size(); ++j) {
                sum_sq += result[j] * result[j];
            }
            BOOST_CHECK_CLOSE_FRACTION(sum_sq, 1.0, 1e-5);
        }
    }
}
