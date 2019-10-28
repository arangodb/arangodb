// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
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

class FromInt {
    int data_;
public:
    constexpr FromInt(int data) : data_(data) { }
    constexpr bool operator==(const FromInt& x) const { return data_ == x.data_; }
};

int main() {
    //////////////////////////////////////////////////////////////////////////
    // Check for the pair(T&&, U&&) constructor
    //////////////////////////////////////////////////////////////////////////
    {
        hana::pair<MoveOnly, short*> p(MoveOnly{3}, nullptr);
        BOOST_HANA_RUNTIME_CHECK(hana::first(p) == MoveOnly{3});
        BOOST_HANA_RUNTIME_CHECK(hana::second(p) == nullptr);
    }

    //////////////////////////////////////////////////////////////////////////
    // Check for the pair(First const&, Second const&) constructor
    //////////////////////////////////////////////////////////////////////////
    {
        hana::pair<float, short*> p1(3.5f, 0);
        BOOST_HANA_RUNTIME_CHECK(hana::first(p1) == 3.5f);
        BOOST_HANA_RUNTIME_CHECK(hana::second(p1) == nullptr);

        // brace init
        hana::pair<float, short*> p2 = {3.5f, 0};
        BOOST_HANA_RUNTIME_CHECK(hana::first(p2) == 3.5f);
        BOOST_HANA_RUNTIME_CHECK(hana::second(p2) == nullptr);
    }
    {
        hana::pair<FromInt, int> p1(1, 2);
        BOOST_HANA_RUNTIME_CHECK(hana::first(p1) == FromInt(1));
        BOOST_HANA_RUNTIME_CHECK(hana::second(p1) == 2);

        // brace init
        hana::pair<FromInt, int> p2 = {1, 2};
        BOOST_HANA_RUNTIME_CHECK(hana::first(p2) == FromInt(1));
        BOOST_HANA_RUNTIME_CHECK(hana::second(p2) == 2);
    }

    // Make sure the above works constexpr too
    {
        constexpr hana::pair<float, short*> p1(3.5f, 0);
        static_assert(hana::first(p1) == 3.5f, "");
        static_assert(hana::second(p1) == nullptr, "");

        // brace init
        constexpr hana::pair<float, short*> p2 = {3.5f, 0};
        static_assert(hana::first(p2) == 3.5f, "");
        static_assert(hana::second(p2) == nullptr, "");
    }
    {
        constexpr hana::pair<FromInt, int> p1(1, 2);
        static_assert(hana::first(p1) == FromInt(1), "");
        static_assert(hana::second(p1) == 2, "");

        // brace init
        constexpr hana::pair<FromInt, int> p2 = {1, 2};
        static_assert(hana::first(p2) == FromInt(1), "");
        static_assert(hana::second(p2) == 2, "");
    }
}
