// Boost.Range library
//
// Copyright Neil Groves 2014. Use, modification and distribution is subject
// to the Boost Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org/libs/range
//

#include "mock_range.hpp"
#include <boost/range/adaptor/reversed.hpp>

int main(int, const char**)
{
    using boost::range::unit_test::mock_range;
    using boost::adaptors::reversed;

    // This next line should fail when Boost.Range concept checking is
    // enabled since the adaptor takes at least a BidirectionalRange.
    return (mock_range<boost::forward_traversal_tag>() | reversed).front();
}
 
