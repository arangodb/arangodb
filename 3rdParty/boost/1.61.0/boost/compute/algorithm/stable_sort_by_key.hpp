//---------------------------------------------------------------------------//
// Copyright (c) 2016 Jakub Szuppe <j.szuppe@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_ALGORITHM_STABLE_SORT_BY_KEY_HPP
#define BOOST_COMPUTE_ALGORITHM_STABLE_SORT_BY_KEY_HPP

#include <iterator>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/sort_by_key.hpp>
#include <boost/compute/detail/iterator_range_size.hpp>

namespace boost {
namespace compute {

/// Performs a key-value stable sort using the keys in the range [\p keys_first,
/// \p keys_last) on the values in the range [\p values_first,
/// \p values_first \c + (\p keys_last \c - \p keys_first)) using \p compare.
///
/// If no compare function is specified, \c less is used.
///
/// \see sort()
template<class KeyIterator, class ValueIterator, class Compare>
inline void stable_sort_by_key(KeyIterator keys_first,
                               KeyIterator keys_last,
                               ValueIterator values_first,
                               Compare compare,
                               command_queue &queue = system::default_queue())
{
    // sort_by_key is stable
    ::boost::compute::sort_by_key(
        keys_first, keys_last, values_first, compare, queue
    );
}

/// \overload
template<class KeyIterator, class ValueIterator>
inline void stable_sort_by_key(KeyIterator keys_first,
                               KeyIterator keys_last,
                               ValueIterator values_first,
                               command_queue &queue = system::default_queue())
{
    typedef typename std::iterator_traits<KeyIterator>::value_type key_type;

    ::boost::compute::stable_sort_by_key(
        keys_first, keys_last, values_first, less<key_type>(), queue
    );
}

} // end compute namespace
} // end boost namespace

#endif // BOOST_COMPUTE_ALGORITHM_STABLE_SORT_BY_KEY_HPP
