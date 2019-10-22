// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/mult.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
#include <boost/hana/unpack.hpp>
#include <boost/hana/zip_with.hpp>

#include <tuple>
#include <type_traits>
#include <utility>
namespace hana = boost::hana;


// Basic usage:
static_assert(
    hana::zip_with(hana::mult, hana::make_tuple(1, 2, 3, 4), hana::make_tuple(5, 6, 7, 8))
    ==
    hana::make_tuple(5, 12, 21, 32)
, "");



// Example of computing a tuple of all the common types of several tuples:
template<typename... Ts>
using common_tuple_t = typename decltype(
    hana::unpack(
        hana::zip_with(
            hana::metafunction<std::common_type>,
            hana::transform(std::declval<Ts>(), hana::decltype_)...
        ),
        hana::template_<std::tuple>
    )
)::type;


static_assert(std::is_same<
    common_tuple_t<
        std::tuple<bool, int, unsigned>,
        std::tuple<char, long, long>,
        std::tuple<int, long long, double>
    >,
    std::tuple<int, long long, double>
>::value, "");

int main() { }
