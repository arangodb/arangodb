// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include "measure.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>


int main () {
    boost::hana::benchmark::measure([] {
        long long result = 0;
        for (int iteration = 0; iteration < 1 << 10; ++iteration) {
            std::array<int, <%= input_size %>> values = {{
                <%= input_size.times.map { 'std::rand()' }.join(', ') %>
            }};

            std::array<long long, <%= input_size %>> results{};

            std::transform(values.begin(), values.end(), results.begin(), [&](auto t) {
                return result += t;
            });
        }
    });
}
