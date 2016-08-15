// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/fusion/include/make_list.hpp>
#include <boost/fusion/include/push_back.hpp>


template <int i>
struct x { };

int main() {
    auto xs = <%= fusion_list((1..input_size).map { |n| "x<#{n}>{}" }) %>;
    (void)xs;
}
