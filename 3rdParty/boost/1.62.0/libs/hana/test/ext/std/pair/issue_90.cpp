// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/std/pair.hpp>
#include <boost/hana/first.hpp>
#include <boost/hana/second.hpp>

#include <utility>
namespace hana = boost::hana;


template <typename T>
T const& cref(T& t) { return t; }

// a non-movable, non-copyable type
template <int i>
struct RefOnly {
    RefOnly() = default;
    RefOnly(RefOnly const&) = delete;
    RefOnly(RefOnly&&) = delete;
};

int main() {
    // This test makes sure that we return the proper reference types from
    // `first` and `second`.
    std::pair<RefOnly<0>, RefOnly<1>> p;

    {
        RefOnly<0>&& r1 = hana::first(std::move(p));
        RefOnly<0>& r2 = hana::first(p);
        RefOnly<0> const& r3 = hana::first(cref(p));

        (void)r1; (void)r2; (void)r3;
    }

    {
        RefOnly<1>&& r1 = hana::second(std::move(p));
        RefOnly<1>& r2 = hana::second(p);
        RefOnly<1> const& r3 = hana::second(cref(p));

        (void)r1; (void)r2; (void)r3;
    }
}
