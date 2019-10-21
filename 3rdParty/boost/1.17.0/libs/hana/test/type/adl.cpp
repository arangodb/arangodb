// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/type.hpp>
namespace hana = boost::hana;


template <bool b = false>
struct invalid { static_assert(b, "invalid must not be instantiated"); };

template <typename T> void adl(T) { }
template <typename T> void adl_pattern(hana::basic_type<T>) { }


int main() {
    // ADL kicks in but `invalid<>` must not instantiated
    adl(hana::type_c<invalid<>>);
    adl_pattern(hana::type_c<invalid<>>);

    // ADL instantiates the types recursively, make sure that works too
    adl(hana::typeid_(hana::type_c<invalid<>>));
    adl_pattern(hana::typeid_(hana::type_c<invalid<>>));
}
