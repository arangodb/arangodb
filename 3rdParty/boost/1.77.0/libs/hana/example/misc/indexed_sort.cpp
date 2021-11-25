// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/at.hpp>
#include <boost/hana/back.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/front.hpp>
#include <boost/hana/functional/on.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/length.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/range.hpp>
#include <boost/hana/sort.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
#include <boost/hana/zip.hpp>

#include <type_traits>
namespace hana = boost::hana;
using namespace hana::literals;


auto indexed_sort = [](auto list, auto predicate) {
    auto indexed_list = hana::zip(
        list,
        hana::to_tuple(hana::make_range(0_c, hana::length(list)))
    );
    auto sorted = hana::sort.by(predicate ^hana::on^ hana::front, indexed_list);
    return hana::make_pair(hana::transform(sorted, hana::front),
                           hana::transform(sorted, hana::back));
};

int main() {
    auto types = hana::tuple_t<char[4], char[2], char[1], char[5], char[3]>;
    auto sorted = indexed_sort(types, [](auto t, auto u) {
        return hana::sizeof_(t) < hana::sizeof_(u);
    });
    using Tup = decltype(
        hana::unpack(hana::first(sorted), hana::template_<hana::tuple>)
    )::type;
    auto indices = hana::second(indexed_sort(hana::second(sorted), hana::less));

    // When accessed through the indices sequence, the tuple appears to be
    // ordered as the `types` above. However, as can be seen in the
    // static_assert below, the tuple is actually ordered differently.
    Tup tup;
    char const(&a)[4] = tup[indices[0_c]];
    char const(&b)[2] = tup[indices[1_c]];
    char const(&c)[1] = tup[indices[2_c]];
    char const(&d)[5] = tup[indices[3_c]];
    char const(&e)[3] = tup[indices[4_c]];

    static_assert(std::is_same<
        Tup,
        hana::tuple<char[1], char[2], char[3], char[4], char[5]>
    >{}, "");

    (void)a; (void)b; (void)c; (void)d; (void)e;
}
