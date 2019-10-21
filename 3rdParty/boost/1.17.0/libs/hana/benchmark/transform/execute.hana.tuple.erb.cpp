// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/transform.hpp>
#include <boost/hana/tuple.hpp>

#include "measure.hpp"
#include <cstdlib>


int main () {
    boost::hana::benchmark::measure([] {
        long long result = 0;
        for (int iteration = 0; iteration < 1 << 10; ++iteration) {
            auto values = boost::hana::make_tuple(
                <%= input_size.times.map { 'std::rand()' }.join(', ') %>
            );

            auto transformed = boost::hana::transform(values, [&](auto t) {
                return result += t;
            });
            (void)transformed;
        }
    });
}
