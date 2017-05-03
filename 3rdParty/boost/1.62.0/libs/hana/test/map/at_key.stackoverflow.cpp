// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/fold_left.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/range.hpp>
namespace hana = boost::hana;


//
// Make sure that http://stackoverflow.com/q/32702383/627587 works.
//

auto at = [](auto& map, auto key) -> auto& {
    return map[key];
};

template <typename Map, typename Keys>
auto& traverse(Map& map, Keys const& keys){
    return hana::fold_left(keys, map, at);
}

int main() {
    auto xs = hana::make_map(hana::make_pair(hana::int_c<0>,
                hana::make_map(hana::make_pair(hana::int_c<1>,
                    hana::make_map(hana::make_pair(hana::int_c<2>,
                        hana::make_map(hana::make_pair(hana::int_c<3>, 10))))))));

    int& i = traverse(xs, hana::range_c<int, 0, 4>);
    BOOST_HANA_RUNTIME_CHECK(i == 10);
    i = 99;
    BOOST_HANA_RUNTIME_CHECK(traverse(xs, hana::range_c<int, 0, 4>) == 99);
}
