// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/first.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/second.hpp>

#include <type_traits>
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

template <typename Target>
struct implicit_to {
    constexpr operator Target() const { return Target{}; }
};

struct NoMove {
    NoMove() = default;
    NoMove(NoMove const&) = delete;
    NoMove(NoMove&&) = delete;
};

// Note: It is also useful to check with a non-empty class, because that
//       triggers different instantiations due to EBO.
struct NoMove_nonempty {
    NoMove_nonempty() = default;
    NoMove_nonempty(NoMove_nonempty const&) = delete;
    NoMove_nonempty(NoMove_nonempty&&) = delete;
    int i;
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
    {
        struct target1 {
            target1() = default;
            target1(target1 const&) = delete;
            target1(target1&&) = default;
        };

        struct target2 {
            target2() = default;
            target2(target2 const&) = delete;
            target2(target2&&) = default;
        };
        using Target = hana::pair<target1, target2>;
        Target p1(hana::make_pair(target1{}, target2{})); (void)p1;
        Target p2(hana::make_pair(implicit_to<target1>{}, target2{})); (void)p2;
        Target p3(hana::make_pair(target1{}, implicit_to<target2>{})); (void)p3;
        Target p4(hana::make_pair(implicit_to<target1>{}, implicit_to<target2>{})); (void)p4;
    }

    // Make sure we don't define the move constructor when it shouldn't be defined.
    {
        using Pair1 = hana::pair<NoMove, NoMove>;
        Pair1 pair1; (void)pair1;
        static_assert(!std::is_move_constructible<Pair1>::value, "");

        using Pair2 = hana::pair<NoMove_nonempty, NoMove_nonempty>;
        Pair2 pair2; (void)pair2;
        static_assert(!std::is_move_constructible<Pair2>::value, "");
    }
}
