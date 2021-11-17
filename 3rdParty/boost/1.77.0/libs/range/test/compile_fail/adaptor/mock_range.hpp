// Boost.Range library
//
// Copyright Neil Groves 2014. Use, modification and distribution is subject
// to the Boost Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org/libs/range
//
#ifndef BOOST_RANGE_UNIT_TEST_ADAPTOR_MOCK_RANGE_HPP_INCLUDED
#define BOOST_RANGE_UNIT_TEST_ADAPTOR_MOCK_RANGE_HPP_INCLUDED

#include "mock_iterator.hpp"
#include <boost/range/iterator_range_core.hpp>

namespace boost
{
    namespace range
    {
        namespace unit_test
        {

// Make a non-empty range that models the corresponding range concept.
// This is only useful in unit tests. It is main use is to help test concepts
// assertions are present.
template<typename TraversalTag>
iterator_range<mock_iterator<TraversalTag> >&
    mock_range()
{
    static iterator_range<mock_iterator<TraversalTag> > instance(
                mock_iterator<TraversalTag>(0),
                mock_iterator<TraversalTag>(1));
    return instance;
}

template<typename TraversalTag>
const iterator_range<mock_iterator<TraversalTag> >&
    mock_const_range()
{
    static iterator_range<mock_iterator<TraversalTag> > instance(
                mock_iterator<TraversalTag>(0),
                mock_iterator<TraversalTag>(1));
    return instance;
}

        } // namespace unit_test
    } // namespace range
} // namespace boost

#endif // include guard
