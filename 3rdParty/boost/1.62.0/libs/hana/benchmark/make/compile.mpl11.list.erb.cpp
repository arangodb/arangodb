// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/mpl11/list.hpp>


template <int>
struct x;

using list = boost::mpl11::list<
    <%= (1..input_size).map { |i| "x<#{i}>" }.join(', ') %>
>;

int main() {

}
