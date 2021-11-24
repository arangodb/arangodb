/* Copyright (C) 2016 Edward Diener
* 
* Use, modification and distribution is subject to the 
* Boost Software License, Version 1.0. (See accompanying
* file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef BOOST_POOL_TEST_RANDOM_SHUFFLE_HPP
#define BOOST_POOL_TEST_RANDOM_SHUFFLE_HPP

#include <cstdlib>
#include <iterator>
#include <algorithm>

template< class RandomIt >
void pool_test_random_shuffle( RandomIt first, RandomIt last )
{
    typename std::iterator_traits<RandomIt>::difference_type i, n;
    n = last - first;
    for (i = n-1; i > 0; --i) {
        using std::swap;
        swap(first[i], first[std::rand() % (i+1)]);
    }
}

#endif // BOOST_POOL_TEST_RANDOM_SHUFFLE_HPP
