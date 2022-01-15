// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <array>


template <int i>
struct x { };

int main() {
    constexpr std::array<int, <%= input_size %>> array = {{
        <%= (1..input_size).to_a.join(', ') %>
    }};
    (void)array;
}
