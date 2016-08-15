// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <meta/meta.hpp>


template <int>
struct x;

using list = meta::list<
    <%= (1..input_size).map { |i| "x<#{i}>" }.join(', ') %>
>;

using result = meta::at_c<list, <%= input_size-1 %>>;


int main() { }
