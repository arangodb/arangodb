// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/fusion/include/make_map.hpp>

#include "../../at_key/pair.hpp"
namespace fusion = boost::fusion;


template <int i>
struct x { };

struct undefined { };

int main() {
    auto map = fusion::make_map<<%=
        (1..input_size).map { |n| "x<#{n}>" }.join(', ')
    %>>(<%=
        (1..input_size).map { |n| "undefined{}" }.join(', ')
    %>);
    (void)map;
}
