/* test_hyperexponential.cpp
 *
 * Copyright Marco Guazzone 2014
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * \author Marco Guazzone (marco.guazzone@gmail.com)
 *
 */

#include <boost/exception/diagnostic_information.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/math/distributions/hyperexponential.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/random/hyperexponential_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/range/numeric.hpp>
#include <cstring>
#include <iostream>
#include <numeric>
#include <vector>

#include "statistic_tests.hpp"


// This test unit is similar to the one in "test_real_distribution.ipp", which
// has been changed for the hyperexponential distribution.
// We cannot directly use the original test suite since it doesn't work for
// distributions with vector parameters.
// Also, this implementation has been inspired by the test unit for the
// discrete_distribution class.


#ifndef BOOST_RANDOM_P_CUTOFF
# define BOOST_RANDOM_P_CUTOFF 0.99
#endif

#define BOOST_RANDOM_HYPEREXP_NUM_PHASES_MIN 1
#define BOOST_RANDOM_HYPEREXP_NUM_PHASES_MAX 3
#define BOOST_RANDOM_HYPEREXP_PROBABILITY_MIN 0.01
#define BOOST_RANDOM_HYPEREXP_PROBABILITY_MAX 1.0
#define BOOST_RANDOM_HYPEREXP_RATE_MIN 0.0001
#define BOOST_RANDOM_HYPEREXP_RATE_MAX 1000.0
#define BOOST_RANDOM_HYPEREXP_NUM_TRIALS 1000000ll


namespace /*<unnamed>*/ { namespace detail {

template <typename T>
std::vector<T>& normalize(std::vector<T>& v)
{
	if (v.size() == 0)
	{
		return v;
	}

    const T sum = std::accumulate(v.begin(), v.end(), static_cast<T>(0));
	T final_sum = 0;

    const typename std::vector<T>::iterator end = --v.end();
    for (typename std::vector<T>::iterator it = v.begin();
         it != end;
         ++it)
    {
        *it /= sum;
    }
	*end = 1 - final_sum;  // avoids round off errors, ensures the probs really do sum to 1.

    return v;
}

template <typename T>
std::vector<T> normalize_copy(std::vector<T> const& v)
{
	std::vector<T> vv(v);

    return normalize(vv);
}

template <typename T, typename DistT, typename EngineT>
std::vector<T> make_random_vector(std::size_t n, DistT const& dist, EngineT& eng)
{
    std::vector<T> res(n);

    for (std::size_t i = 0; i < n; ++i)
    {
        res[i] = dist(eng);
    }

    return res;
}

}} // Namespace <unnamed>::detail


bool do_test(std::vector<double> const& probabilities,
             std::vector<double> const& rates,
             long long max,
             boost::mt19937& gen)
{
    std::cout << "running hyperexponential(probabilities,rates) " << max << " times: " << std::flush;

    boost::math::hyperexponential_distribution<> expected(probabilities, rates);

    boost::random::hyperexponential_distribution<> dist(probabilities, rates);

    kolmogorov_experiment test(boost::numeric_cast<int>(max));
    boost::variate_generator<boost::mt19937&, boost::random::hyperexponential_distribution<> > vgen(gen, dist);

    const double prob = test.probability(test.run(vgen, expected));

    const bool result = prob < BOOST_RANDOM_P_CUTOFF;
    const char* err = result? "" : "*";
    std::cout << std::setprecision(17) << prob << err << std::endl;

    std::cout << std::setprecision(6);

    return result;
}

bool do_tests(int repeat, int max_num_phases, double max_rate, long long trials)
{
    boost::mt19937 gen;
    boost::random::uniform_int_distribution<> num_phases_dist(BOOST_RANDOM_HYPEREXP_NUM_PHASES_MIN, max_num_phases);

    int errors = 0;
    for (int i = 0; i < repeat; ++i)
    {
        const int num_phases = num_phases_dist(gen);
        boost::random::uniform_real_distribution<> prob_dist(BOOST_RANDOM_HYPEREXP_PROBABILITY_MIN, BOOST_RANDOM_HYPEREXP_PROBABILITY_MAX);
        boost::random::uniform_real_distribution<> rate_dist(BOOST_RANDOM_HYPEREXP_RATE_MIN, max_rate);

        const std::vector<double> probabilities = detail::normalize_copy(detail::make_random_vector<double>(num_phases, prob_dist, gen));
        const std::vector<double> rates = detail::make_random_vector<double>(num_phases, rate_dist, gen);

        if (!do_test(probabilities, rates, trials, gen))
        {
            ++errors;
        }
    }
    if (errors != 0)
    {
        std::cout << "*** " << errors << " errors detected ***" << std::endl;
    }

    return errors == 0;
}

int usage()
{
    std::cerr << "Usage: test_hyperexponential"
        " -r <repeat>"
        " -num_phases"
        " <max num_phases>"
        " -rate"
        " <max rate>"
        " -t <trials>" << std::endl;
    return 2;
}

template<class T>
bool handle_option(int& argc, char**& argv, const char* opt, T& value)
{
    if (std::strcmp(argv[0], opt) == 0 && argc > 1)
    {
        --argc;
        ++argv;
        value = boost::lexical_cast<T>(argv[0]);
        return true;
    }
    else
    {
        return false;
    }
}

int main(int argc, char** argv)
{
    int repeat = 1;
    int max_num_phases = BOOST_RANDOM_HYPEREXP_NUM_PHASES_MAX;
    double max_rate = BOOST_RANDOM_HYPEREXP_RATE_MAX;
    long long trials = BOOST_RANDOM_HYPEREXP_NUM_TRIALS;

    if (argc > 0)
    {
        --argc;
        ++argv;
    }
    while(argc > 0)
    {
        if (argv[0][0] != '-')
        {
            return usage();
        }
        else if (!handle_option(argc, argv, "-r", repeat)
                 && !handle_option(argc, argv, "-num_phases", max_num_phases)
                 && !handle_option(argc, argv, "-rate", max_rate)
                 && !handle_option(argc, argv, "-t", trials))
        {
            return usage();
        }
        --argc;
        ++argv;
    }

    try
    {
        if (do_tests(repeat, max_num_phases, max_rate, trials))
        {
            return 0;
        }
        else
        {
            return EXIT_FAILURE;
        }
    }
    catch(...)
    {
        std::cerr << boost::current_exception_diagnostic_information() << std::endl;
        return EXIT_FAILURE;
    }
}
