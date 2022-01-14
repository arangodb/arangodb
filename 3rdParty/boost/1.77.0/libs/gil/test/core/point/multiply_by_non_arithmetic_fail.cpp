//
// Copyright 2018 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/point.hpp>

#include <type_traits>

namespace gil = boost::gil;

struct FakeMatrix {};

int main()
{
    gil::point<int> p1{2, 4};

    FakeMatrix m1;
    auto r1 = p1 * m1;
    auto r2 = m1 * p1;
}
