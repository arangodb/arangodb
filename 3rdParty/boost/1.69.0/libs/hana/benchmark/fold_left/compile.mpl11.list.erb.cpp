// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/mpl11/list.hpp>


struct f {
    using type = f;
    template <typename, typename>
    struct apply { struct type; };
};

template <int> struct x { struct type; };

struct state { struct type; };

using list = boost::mpl11::list<
    <%= (1..input_size).map { |i| "x<#{i}>" }.join(', ') %>
>;

using result = boost::mpl11::foldl<f, state, list>::type;

int main() {

}
