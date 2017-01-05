// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/mpl/find_if.hpp>
#include <boost/mpl/integral_c.hpp>
#include <boost/mpl/push_back.hpp>
#include <boost/mpl/quote.hpp>
#include <boost/mpl/vector.hpp>
namespace mpl = boost::mpl;


struct is_last {
    template <typename N>
    struct apply
        : mpl::integral_c<bool, N::type::value == <%= input_size %>>
    { };
};

using vector = <%= mpl_vector((1..input_size).to_a.map { |n|
    "mpl::integral_c<int, #{n}>"
}) %>;

using result = mpl::find_if<vector, is_last>::type;


int main() { }
