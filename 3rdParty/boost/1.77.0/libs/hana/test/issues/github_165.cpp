// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/tuple.hpp>

#include <utility>
namespace hana = boost::hana;


constexpr int f() {
    // copy-assign
    {
        hana::tuple<int> xs{1};
        hana::tuple<long> ys{1};
        xs = xs;
        ys = xs;
    }

    // move-assign
    {
        hana::tuple<int> xs{1}, xxs{1};
        hana::tuple<long> ys{1};
        xs = std::move(xxs);
        ys = std::move(xs);
    }

    return 0;
}

static_assert(f() == 0, ""); // force f() to be constexpr


int main() { }
