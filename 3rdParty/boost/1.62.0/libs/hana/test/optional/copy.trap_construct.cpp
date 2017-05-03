// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/optional.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;


// This test makes sure that we do not instantiate rogue contructors when
// trying to copy a hana::just.

int main() {
    auto just = hana::just(hana::test::trap_construct{});
    auto implicit_copy = just;
    decltype(just) explicit_copy(just);

    (void)implicit_copy;
    (void)explicit_copy;
}
