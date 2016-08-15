// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/at_key.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


int main() {
    auto tuple = hana::make_tuple(<%=
        env[:range].map { |n| "hana::int_<#{n}>{}" }.join(', ')
    %>);

    hana::at_key(tuple, hana::int_c<<%= input_size %>>);
}
