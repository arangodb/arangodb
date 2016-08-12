// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/tuple.hpp>

#include <utility>
namespace hana = boost::hana;


// a non-movable, non-copyable type
struct RefOnly {
    RefOnly() = default;
    RefOnly(RefOnly const&) = delete;
    RefOnly(RefOnly&&) = delete;
};

struct Empty { };

int main() {
    // Make sure we can create a tuple of rvalue references
    {
        RefOnly e{};
        hana::tuple<RefOnly&&> xs{std::move(e)};
        hana::tuple<RefOnly&&, RefOnly&&> ys{std::move(e), std::move(e)};
        (void)xs; (void)ys;

        hana::tuple<RefOnly&&> xs2 = {std::move(e)};
        hana::tuple<RefOnly&&, RefOnly&&> ys2 = {std::move(e), std::move(e)};
        (void)xs2; (void)ys2;
    }

    // Make sure we can create a tuple of non-const lvalue references
    {
        RefOnly e{};
        hana::tuple<RefOnly&> xs{e};
        hana::tuple<RefOnly&, RefOnly&> ys{e, e};
        (void)xs; (void)ys;

        hana::tuple<RefOnly&> xs2 = {e};
        hana::tuple<RefOnly&, RefOnly&> ys2 = {e, e};
        (void)xs2; (void)ys2;
    }

    // Make sure we can create a tuple of const lvalue references
    {
        RefOnly const e{};
        hana::tuple<RefOnly const&> xs{e};
        hana::tuple<RefOnly const&, RefOnly const&> ys{e, e};
        (void)xs; (void)ys;

        hana::tuple<RefOnly const&> xs2 = {e};
        hana::tuple<RefOnly const&, RefOnly const&> ys2 = {e, e};
        (void)xs2; (void)ys2;
    }

    // Try to construct tuples with mixed references and non-reference members.
    {
        RefOnly r{};
        Empty e{};

        {
            hana::tuple<RefOnly const&, Empty> xs{r, e};
            hana::tuple<RefOnly const&, Empty> ys = {r, e};
            (void)xs; (void)ys;
        }
        {
            hana::tuple<RefOnly&, Empty> xs{r, e};
            hana::tuple<RefOnly&, Empty> ys = {r, e};
            (void)xs; (void)ys;
        }
        {
            hana::tuple<RefOnly&&, Empty> xs{std::move(r), e};
            hana::tuple<RefOnly&&, Empty> ys = {std::move(r), e};
            (void)xs; (void)ys;
        }
    }
}
