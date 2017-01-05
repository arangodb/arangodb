// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/first.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/second.hpp>

#include <utility>
namespace hana = boost::hana;


struct MoveOnly {
    int data_;
    MoveOnly(MoveOnly const&) = delete;
    MoveOnly& operator=(MoveOnly const&) = delete;
    MoveOnly(int data = 1) : data_(data) { }
    MoveOnly(MoveOnly&& x) : data_(x.data_) { x.data_ = 0; }

    MoveOnly& operator=(MoveOnly&& x)
    { data_ = x.data_; x.data_ = 0; return *this; }

    bool operator==(const MoveOnly& x) const { return data_ == x.data_; }
};

struct MoveOnlyDerived : MoveOnly {
    MoveOnlyDerived(MoveOnlyDerived&&) = default;
    MoveOnlyDerived(int data = 1) : MoveOnly(data) { }
};

constexpr auto in_constexpr_context(int a, short b) {
    hana::pair<int, short> p1(a, b);
    hana::pair<int, short> p2;
    hana::pair<double, long> p3;
    p2 = std::move(p1);
    p3 = std::move(p2);
    return p3;
}

int main() {
    // from pair<T, U> to pair<T, U>
    {
        hana::pair<MoveOnly, short> p1(MoveOnly{3}, 4);
        hana::pair<MoveOnly, short> p2;
        p2 = std::move(p1);
        BOOST_HANA_RUNTIME_CHECK(hana::first(p2) == MoveOnly{3});
        BOOST_HANA_RUNTIME_CHECK(hana::second(p2) == 4);
    }

    // from pair<T, U> to pair<V, W>
    {
        hana::pair<MoveOnlyDerived, short> p1(MoveOnlyDerived{3}, 4);
        hana::pair<MoveOnly, long> p2;
        p2 = std::move(p1);
        BOOST_HANA_RUNTIME_CHECK(hana::first(p2) == MoveOnly{3});
        BOOST_HANA_RUNTIME_CHECK(hana::second(p2) == 4);
    }

    // make sure that also works in a constexpr context
    {
        constexpr auto p = in_constexpr_context(3, 4);
        static_assert(hana::first(p) == 3, "");
        static_assert(hana::second(p) == 4, "");
    }
}
