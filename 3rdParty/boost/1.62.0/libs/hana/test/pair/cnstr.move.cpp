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
    MoveOnly(int data) : data_(data) { }
    MoveOnly(MoveOnly&& x) : data_(x.data_) { x.data_ = 0; }

    MoveOnly& operator=(MoveOnly&& x)
    { data_ = x.data_; x.data_ = 0; return *this; }

    bool operator==(const MoveOnly& x) const { return data_ == x.data_; }
};

struct MoveOnlyDerived : MoveOnly {
    MoveOnlyDerived(MoveOnlyDerived&&) = default;
    MoveOnlyDerived(int data = 1) : MoveOnly(data) { }
};


int main() {
    {
        hana::pair<MoveOnly, short> p1(MoveOnly{3}, 4);
        hana::pair<MoveOnly, short> p2(std::move(p1));
        BOOST_HANA_RUNTIME_CHECK(hana::first(p2) == MoveOnly{3});
        BOOST_HANA_RUNTIME_CHECK(hana::second(p2) == 4);
    }

    // Make sure it works across pair types
    {
        hana::pair<MoveOnlyDerived, short> p1(MoveOnlyDerived{3}, 4);
        hana::pair<MoveOnly, long> p2 = std::move(p1);
        BOOST_HANA_RUNTIME_CHECK(hana::first(p2) == MoveOnly{3});
        BOOST_HANA_RUNTIME_CHECK(hana::second(p2) == 4);
    }
}
