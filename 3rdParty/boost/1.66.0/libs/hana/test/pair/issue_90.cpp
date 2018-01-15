// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/first.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/second.hpp>

#include <utility>
namespace hana = boost::hana;


template <typename T>
T const& cref(T& t) { return t; }

// a non-movable, non-copyable type
struct RefOnly {
    RefOnly() = default;
    RefOnly(RefOnly const&) = delete;
    RefOnly(RefOnly&&) = delete;
};

int main() {
    // This test makes sure that we return the proper reference types from
    // `first` and `second`.
    hana::pair<RefOnly, RefOnly> p;

    {
        RefOnly&& r1 = hana::first(std::move(p));
        RefOnly& r2 = hana::first(p);
        RefOnly const& r3 = hana::first(cref(p));

        (void)r1; (void)r2; (void)r3;
    }

    {
        RefOnly&& r1 = hana::second(std::move(p));
        RefOnly& r2 = hana::second(p);
        RefOnly const& r3 = hana::second(cref(p));

        (void)r1; (void)r2; (void)r3;
    }
}
