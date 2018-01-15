// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/reverse.hpp>
#include <boost/hana/tuple.hpp>

#include "measure.hpp"
#include <cstdlib>
#include <string>
#include <utility>
namespace hana = boost::hana;


int main () {
    std::string s(1000, 'x');
    hana::benchmark::measure([&] {
        for (int iteration = 0; iteration < 1 << 5; ++iteration) {
            auto values = hana::make_tuple(
                <%= input_size.times.map { 's' }.join(', ') %>
            );

            auto result = hana::reverse(std::move(values));
            (void)result;
        }
    });
}
