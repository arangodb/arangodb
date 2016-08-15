// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

<% if input_size > 10 %>
    #define FUSION_MAX_VECTOR_SIZE <%= ((input_size + 9) / 10) * 10 %>
<% end %>

#include <boost/fusion/include/at.hpp>
#include <boost/fusion/include/make_vector.hpp>
namespace fusion = boost::fusion;


template <int>
struct x { };

int main() {
    auto vector = fusion::make_vector(
        <%= (1..input_size).map { |n| "x<#{n}>{}" }.join(', ') %>
    );
    auto result = fusion::at_c<<%= input_size-1 %>>(vector);
    (void)result;
}
