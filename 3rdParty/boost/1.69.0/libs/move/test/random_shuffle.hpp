#ifndef BOOST_MOVE_TEST_RANDOM_SHUFFLE_HPP
#define BOOST_MOVE_TEST_RANDOM_SHUFFLE_HPP


#include <boost/move/adl_move_swap.hpp>
#include <boost/move/detail/iterator_traits.hpp>
#include <stdlib.h>

template< class RandomIt >
void random_shuffle( RandomIt first, RandomIt last )
{
   typedef typename boost::movelib::iterator_traits<RandomIt>::difference_type difference_type;
   difference_type n = last - first;
   for (difference_type i = n-1; i > 0; --i) {
      difference_type j = std::rand() % (i+1);
      if(j != i) {
         boost::adl_move_swap(first[i], first[j]);
      }
   }
}


#endif// BOOST_MOVE_TEST_RANDOM_SHUFFLE_HPP
