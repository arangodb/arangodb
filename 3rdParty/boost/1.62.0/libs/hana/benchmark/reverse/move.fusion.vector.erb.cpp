// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

<% if input_size > 10 %>
    #define FUSION_MAX_VECTOR_SIZE <%= ((input_size + 9) / 10) * 10 %>
<% end %>

#include <boost/fusion/include/as_vector.hpp>
#include <boost/fusion/include/make_vector.hpp>
#include <boost/fusion/include/reverse.hpp>

#include "measure.hpp"
#include <cstdlib>
#include <string>
#include <utility>
namespace fusion = boost::fusion;
namespace hana = boost::hana;


int main () {
    std::string s(1000, 'x');
    hana::benchmark::measure([&] {
        for (int iteration = 0; iteration < 1 << 5; ++iteration) {
            auto values = fusion::make_vector(
                <%= input_size.times.map { 's' }.join(', ') %>
            );

            auto result = fusion::as_vector(fusion::reverse(std::move(values)));
            (void)result;
        }
    });
}
