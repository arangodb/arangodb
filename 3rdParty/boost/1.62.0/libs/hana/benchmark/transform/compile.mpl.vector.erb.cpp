// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/mpl/push_back.hpp>
#include <boost/mpl/quote.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/vector.hpp>


template <typename X>
struct f { using type = X; };

template <int i>
struct t { };

using vector = <%= mpl_vector((1..input_size).to_a.map { |n| "t<#{n}>" }) %>;

using result = boost::mpl::transform<vector, boost::mpl::quote1<f>>::type;


int main() { }
