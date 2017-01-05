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
#include <boost/range/adaptor/adjacent_filtered.hpp>

namespace
{

struct always_true
{
    typedef bool result_type;

    bool operator()(int, int) const
    {
        return true;
    }
};

} // anonymous namespace

int main(int, const char**)
{
    using boost::range::unit_test::mock_const_range;
    using boost::adaptors::adjacent_filter;

    // This next line should fail when Boost.Range concept checking is
    // enabled since the adjacent_filtered adaptor takes at least a
    // ForwardRange.
    return adjacent_filter(
            mock_const_range<boost::single_pass_traversal_tag>(),
            always_true()).front();
}
