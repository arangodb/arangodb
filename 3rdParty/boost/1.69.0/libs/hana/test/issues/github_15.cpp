// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/functional/always.hpp>

#include <support/tracked.hpp>

#include <utility>
namespace hana = boost::hana;


template <typename T>
decltype(auto) call_always(T&& x) {
    return hana::always(std::forward<T>(x))();
}

int main() {
    auto copy = call_always(Tracked{1});
    (void)copy;
}
