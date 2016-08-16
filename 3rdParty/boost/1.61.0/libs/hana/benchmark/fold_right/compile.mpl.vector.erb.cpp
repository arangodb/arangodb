// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/mpl/push_back.hpp>
#include <boost/mpl/quote.hpp>
#include <boost/mpl/reverse_fold.hpp>
#include <boost/mpl/vector.hpp>


template <typename State, typename X>
struct f { using type = X; };

struct state { };

template <int i>
struct t { };

using vector = <%= mpl_vector((1..input_size).to_a.map { |n| "t<#{n}>" }) %>;

using result = boost::mpl::reverse_fold<
    vector, state, boost::mpl::quote2<f>
>::type;


int main() { }
