//
// Copyright 2019 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/extension/numeric/kernel.hpp>

namespace gil = boost::gil;

int main()
{
    gil::kernel_1d_fixed<int, 0> k0;
    gil::kernel_1d_fixed<int, 4> k4;
}
