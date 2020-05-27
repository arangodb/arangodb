// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>

#include <laws/base.hpp>
#include <support/constexpr_move_only.hpp>
#include <support/minimal_product.hpp>
#include <support/tracked_move_only.hpp>

#include <string>
#include <vector>
namespace hana = boost::hana;


template <int i>
using pair = ::product_t<hana::test::ct_eq<i>, hana::test::ct_eq<-i>>;

// Check with move-only literal types.
constexpr bool in_constexpr_context() {
    hana::map<hana::pair<ConstexprMoveOnly<2>, ConstexprMoveOnly<20>>,
              hana::pair<ConstexprMoveOnly<3>, ConstexprMoveOnly<30>>> map{
        hana::make_pair(ConstexprMoveOnly<2>{}, ConstexprMoveOnly<20>{}),
        hana::make_pair(ConstexprMoveOnly<3>{}, ConstexprMoveOnly<30>{})
    };
    (void)map;
    return true;
}

static_assert(in_constexpr_context(), "");

int main() {
    // Basic check with non trivial runtime state
    {
        std::vector<std::string> v{"Hello", "world", "!"};

        hana::map<
            hana::pair<hana::test::ct_eq<1>, std::string>,
            hana::pair<hana::test::ct_eq<2>, std::vector<std::string>>
        > map{
            hana::make_pair(hana::test::ct_eq<1>{}, std::string{"Hello world!"}),
            hana::make_pair(hana::test::ct_eq<2>{}, v)
        };

        BOOST_HANA_RUNTIME_CHECK(
            hana::at_key(map, hana::test::ct_eq<1>{}) == std::string{"Hello world!"}
        );

        BOOST_HANA_RUNTIME_CHECK(
            hana::at_key(map, hana::test::ct_eq<2>{}) == v
        );
    }

    {
        hana::map<> map{};
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            map,
            hana::make_map()
        ));
    }

    {
        hana::map<pair<0>> map{pair<0>{}};
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            map,
            hana::make_map(pair<0>{})
        ));
    }

    {
        hana::map<pair<0>, pair<1>> map{pair<0>{}, pair<1>{}};
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            map,
            hana::make_map(pair<0>{}, pair<1>{})
        ));
    }

    {
        hana::map<pair<0>, pair<1>, pair<2>> map{pair<0>{}, pair<1>{}, pair<2>{}};
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            map,
            hana::make_map(pair<0>{}, pair<1>{}, pair<2>{})
        ));
    }

    {
        hana::map<pair<0>, pair<1>, pair<2>, pair<3>> map{
            pair<0>{}, pair<1>{}, pair<2>{}, pair<3>{}
        };
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            map,
            hana::make_map(pair<0>{}, pair<1>{}, pair<2>{}, pair<3>{})
        ));
    }

    {
        hana::map<pair<0>, pair<1>, pair<2>, pair<3>, pair<4>> map{
            pair<0>{}, pair<1>{}, pair<2>{}, pair<3>{}, pair<4>{}
        };
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            map,
            hana::make_map(pair<0>{}, pair<1>{}, pair<2>{}, pair<3>{}, pair<4>{})
        ));
    }

    {
        hana::map<pair<0>, pair<1>, pair<2>, pair<3>, pair<4>, pair<5>> map{
            pair<0>{}, pair<1>{}, pair<2>{}, pair<3>{}, pair<4>{}, pair<5>{}
        };
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            map,
            hana::make_map(pair<0>{}, pair<1>{}, pair<2>{}, pair<3>{}, pair<4>{}, pair<5>{})
        ));
    }

    // Use parenthesis syntax instead of braces
    {
        hana::map<> map = hana::map<>();
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            map,
            hana::make_map()
        ));
    }

    {
        hana::map<pair<0>> map(
            pair<0>{}
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            map,
            hana::make_map(pair<0>{})
        ));
    }

    {
        hana::map<pair<0>, pair<1>> map(
            pair<0>{}, pair<1>{}
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            map,
            hana::make_map(pair<0>{}, pair<1>{})
        ));
    }

    {
        hana::map<pair<0>, pair<1>, pair<2>> map(
            pair<0>{}, pair<1>{}, pair<2>{}
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            map,
            hana::make_map(pair<0>{}, pair<1>{}, pair<2>{})
        ));
    }

    {
        hana::map<pair<0>, pair<1>, pair<2>, pair<3>> map(
            pair<0>{}, pair<1>{}, pair<2>{}, pair<3>{}
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            map,
            hana::make_map(pair<0>{}, pair<1>{}, pair<2>{}, pair<3>{})
        ));
    }

    // Check with move-only types
    {
        hana::map<hana::pair<TrackedMoveOnly<1>, TrackedMoveOnly<10>>,
                  hana::pair<TrackedMoveOnly<2>, TrackedMoveOnly<20>>> map{
            hana::make_pair(TrackedMoveOnly<1>{}, TrackedMoveOnly<10>{}),
            hana::make_pair(TrackedMoveOnly<2>{}, TrackedMoveOnly<20>{})
        };
    }

    // The following used to fail when we did not constrain the
    // constructor properly:
    {
        hana::map<pair<1>> map{};
        hana::test::_injection<0> f{};
        auto x = f(map);
    }
}
