// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/mpl11/list.hpp>


struct is_even {
    using type = is_even;
    template <typename N>
    using apply = boost::mpl11::bool_<N::value % 2 == 0>;
};

using list = boost::mpl11::list<
    <%= (1..input_size).map { |i| "boost::mpl11::int_<#{i}>" }.join(', ') %>
>;

using result = boost::mpl11::filter<is_even, list>::type;

int main() { }
