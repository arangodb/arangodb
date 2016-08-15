// Copyright Louis Dionne 2013-2016
// Copyright Zach Laine 2014
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/fold_right.hpp>
#include <boost/hana/tuple.hpp>

#include "measure.hpp"
#include <cstdlib>


int main () {
    boost::hana::benchmark::measure([] {
        long double result = 0;
        for (int iteration = 0; iteration < 1 << 10; ++iteration) {
            auto values = boost::hana::make_tuple(
                <%= input_size.times.map { 'std::rand()' }.join(', ') %>
            );

            result += boost::hana::fold_right(values, 0, [](auto t, auto state) {
                return state + t;
            });
        }
    });
}
