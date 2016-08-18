// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <meta/meta.hpp>


struct is_even {
    template <typename N>
    using apply = meta::bool_<N::value % 2 == 0>;
};

using list = meta::list<
    <%= (1..input_size).map { |i| "meta::int_<#{i}>" }.join(', ') %>
>;

using result = meta::filter<list, is_even>;

int main() {

}
