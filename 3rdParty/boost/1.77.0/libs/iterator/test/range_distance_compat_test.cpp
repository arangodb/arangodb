// Copyright (C) 2018 Andrey Semashev
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/range/distance.hpp>
#include <boost/range/iterator_range_core.hpp>
#include <boost/iterator/distance.hpp>

int main()
{
    // Test that boost::distance from Boost.Range works with boost::distance from Boost.Iterator
    // (https://github.com/boostorg/iterator/commit/b844c8df530c474ec1856870b9b0de5f487b84d4#commitcomment-30603668)

    typedef boost::iterator_range<const char*> range_type;
    range_type range;

    (void)boost::distance(range);

    return 0;
}
