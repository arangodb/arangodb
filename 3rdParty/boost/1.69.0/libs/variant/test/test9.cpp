//-----------------------------------------------------------------------------
// boost-libs variant/test/test9.cpp source file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2016 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


// Test from https://github.com/boostorg/variant/pull/21

#include <boost/fusion/container/vector.hpp>
#include <boost/fusion/mpl.hpp>
#include <boost/variant/variant.hpp>

int main() {
    boost::make_variant_over<boost::fusion::vector<int, char> >::type t;
    (void)t;
}
