#ifndef BOOST_MOVE_TEST_RANDOM_SHUFFLE_HPP
#define BOOST_MOVE_TEST_RANDOM_SHUFFLE_HPP


#include <boost/move/adl_move_swap.hpp>
#include <boost/move/detail/iterator_traits.hpp>
#include <stdlib.h>

inline unsigned long long rand_15_bit()
{
   //Many rand implementation only use 15 bits
   //so make sure we have only 15 bits
   return (unsigned long long)((std::rand()) & 0x7fffu);
}

inline unsigned long long ullrand()
{
   return  (rand_15_bit() << 54u) ^ (rand_15_bit() << 39u) 
         ^ (rand_15_bit() << 26u) ^ (rand_15_bit() << 13u)
         ^ rand_15_bit();
}

template< class RandomIt >
void random_shuffle( RandomIt first, RandomIt last )
{
   typedef typename boost::movelib::iterator_traits<RandomIt>::difference_type difference_type;
   difference_type n = last - first;
   for (difference_type i = n-1; i > 0; --i) {
      difference_type j = ullrand() % (i+1);
      if(j != i) {
         boost::adl_move_swap(first[i], first[j]);
      }
   }
}


#endif// BOOST_MOVE_TEST_RANDOM_SHUFFLE_HPP
