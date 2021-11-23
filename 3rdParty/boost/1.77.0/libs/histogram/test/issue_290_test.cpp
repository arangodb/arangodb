// Copyright 2020 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// - bug only appears on cxxstd=17 or higher and only in gcc
// - reported in issue: https://github.com/boostorg/histogram/issues/290
// - originally caused by rank struct in boost/type_traits/rank.hpp,
//   which we emulate here to avoid the dependency on boost.type_traits

// Original: #include <boost/type_traits/rank.hpp>
template <class T>
struct rank;

#include <boost/histogram/axis/traits.hpp>

int main() { return 0; }
