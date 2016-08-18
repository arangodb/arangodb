// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/at.hpp>
#include <boost/hana/tuple.hpp>

#include "measure.hpp"
#include <cstdlib>
namespace hana = boost::hana;


int main () {
    hana::benchmark::measure([] {
        unsigned long long result = 0;
        for (int iteration = 0; iteration < 1 << 10; ++iteration) {
            auto values = boost::hana::make_tuple(
                <%= input_size.times.map { 'std::rand()' }.join(', ') %>
            );

            <% (0..(input_size-1)).each { |n| %>
                result += hana::at_c<<%= n %>>(values);
            <% } %>
        }
    });
}
