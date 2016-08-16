// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/at_key.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/map.hpp>

#include "../pair.hpp"
namespace hana = boost::hana;


struct undefined { };

int main() {
    auto map = hana::make_map(<%=
        env[:range].map { |n| "light_pair<hana::int_<#{n}>, undefined>{}" }.join(', ')
    %>);

    hana::at_key(map, hana::int_c<<%= input_size %>>);
}
