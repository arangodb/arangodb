// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include "../../at_key/baseline.hpp"
#include "../../at_key/pair.hpp"


template <int i>
struct x { };

struct undefined { };

int main() {
    auto map = baseline::make_map(<%=
        (1..input_size).map { |n| "light_pair<x<#{n}>, undefined>{}" }.join(', ')
    %>);
    (void)map;
}
