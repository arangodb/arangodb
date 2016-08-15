// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/count_if.hpp>
#include <boost/hana/tuple.hpp>

#include "measure.hpp"
#include <cstdlib>
namespace hana = boost::hana;


int main () {
    hana::benchmark::measure([] {
        unsigned long long result = 0;
        for (int iteration = 0; iteration < 1 << 10; ++iteration) {
            auto values = hana::make_tuple(
                <%= input_size.times.map { 'std::rand()' }.join(', ') %>
            );

            result += hana::count_if(values, [](auto i) {
                return i % 2 == 0;
            });
        }
    });
}
