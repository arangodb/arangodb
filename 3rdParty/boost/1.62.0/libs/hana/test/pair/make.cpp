// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/make.hpp>
#include <boost/hana/first.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/second.hpp>
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

int main() {
    {
        hana::pair<int, short> p = hana::make_pair(3, 4);
        BOOST_HANA_RUNTIME_CHECK(hana::first(p) == 3);
        BOOST_HANA_RUNTIME_CHECK(hana::second(p) == 4);
    }

    {
        hana::pair<MoveOnly, short> p = hana::make_pair(MoveOnly{3}, 4);
        BOOST_HANA_RUNTIME_CHECK(hana::first(p) == MoveOnly{3});
        BOOST_HANA_RUNTIME_CHECK(hana::second(p) == 4);
    }

    {
        hana::pair<MoveOnly, short> p = hana::make_pair(3, 4);
        BOOST_HANA_RUNTIME_CHECK(hana::first(p) == MoveOnly{3});
        BOOST_HANA_RUNTIME_CHECK(hana::second(p) == 4);
    }

    {
        constexpr hana::pair<int, short> p = hana::make_pair(3, 4);
        static_assert(hana::first(p) == 3, "");
        static_assert(hana::second(p) == 4, "");
    }

    // equivalence with make<pair_tag>
    {
        constexpr hana::pair<int, short> p = hana::make<hana::pair_tag>(3, 4);
        static_assert(hana::first(p) == 3, "");
        static_assert(hana::second(p) == 4, "");
    }
}
