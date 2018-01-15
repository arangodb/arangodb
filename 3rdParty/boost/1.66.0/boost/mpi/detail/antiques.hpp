//          Copyright Alain Miniussi 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// Authors: Alain Miniussi

#ifndef BOOST_MPI_ANTIQUES_HPP
#define BOOST_MPI_ANTIQUES_HPP

#include <vector>

// Support for some obsolette compilers

namespace boost { namespace mpi {
    namespace detail {
      // Some old gnu compiler have no support for vector<>::data
      // Use this in the mean time, the cumbersome syntax should 
      // serve as an incentive to get rid of this when those compilers 
      // are dropped.
      template <typename T, typename A>
      T* c_data(std::vector<T,A>& v) { return &(v[0]); }

      template <typename T, typename A>
      T const* c_data(std::vector<T,A> const& v) { return &(v[0]); }
  
} } }

#endif
