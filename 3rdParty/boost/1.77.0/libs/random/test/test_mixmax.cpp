/* test_mixmax.cpp
 *
 * Copyright Kostas Savvidis 2019
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * $Id$
 *
 */

#include <boost/random/mixmax.hpp>
#include <cmath>

#define BOOST_RANDOM_URNG boost::random::mixmax

#define BOOST_RANDOM_SEED_WORDS 4

//#define BOOST_RANDOM_VALIDATION_VALUE 1U
#define BOOST_RANDOM_SEED_SEQ_VALIDATION_VALUE 48757672604362303ULL
#define BOOST_RANDOM_ITERATOR_VALIDATION_VALUE 3802490769389764ULL

// 10000th invocation of MIXMAX with N=17, with default constructor should give:
#define BOOST_RANDOM_VALIDATION_VALUE UINT64_C(1842572666014501720)
#define BOOST_RANDOM_GENERATE_VALUES {  3132207748, 2861541672, 3191701354, 4046050275 }

#include "test_generator.ipp"

struct seed_seq_0 {
    template<class It>
    void generate(It begin, It end) const {
        std::fill(begin, end, boost::uint32_t(0xFFFFFFFF));
    }
};

BOOST_AUTO_TEST_CASE(test_special_seed) {
    {
        seed_seq_0 seed;
        std::vector<boost::uint32_t> vec(17);
        seed.generate(vec.begin(), vec.end()); // fill vec with ones
        std::vector<boost::uint32_t>::iterator it = vec.begin();
        boost::random::mixmax gen1(it, vec.end()); // init gen1 with vec iterator
        BOOST_CHECK_EQUAL(gen1(), 775778250716139533ULL);
        BOOST_CHECK_EQUAL(gen1(), 846264592759195742ULL);

        boost::random::mixmax gen2(seed); // init gen2 with seeq_seq, should be the same as gen1!
        BOOST_CHECK_EQUAL(gen2(), 775778250716139533ULL);
        BOOST_CHECK_EQUAL(gen2(), 846264592759195742ULL);

        BOOST_CHECK_EQUAL(gen1, gen2);
    }
}
