// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/comparable.hpp>
#include <laws/constant.hpp>
#include <laws/euclidean_ring.hpp>
#include <laws/group.hpp>
#include <laws/hashable.hpp>
#include <laws/logical.hpp>
#include <laws/monoid.hpp>
#include <laws/orderable.hpp>
#include <laws/ring.hpp>

#include <type_traits>
using namespace boost::hana;


struct inherit_simple : std::integral_constant<int, 3> { };
struct inherit_no_default : std::integral_constant<int, 3> {
    inherit_no_default() = delete;
};

struct incomplete;
struct empty_type { };
struct non_pod { virtual ~non_pod() { } };

int main() {
    auto ints = make<tuple_tag>(
        std::integral_constant<int, -10>{},
        std::integral_constant<int, -2>{},
        std::integral_constant<int, 0>{},
        std::integral_constant<int, 1>{},
        std::integral_constant<int, 3>{}
    );
    (void)ints;

#if BOOST_HANA_TEST_PART == 1
    //////////////////////////////////////////////////////////////////////////
    // Make sure the tag is detected properly
    //////////////////////////////////////////////////////////////////////////
    {
        static_assert(std::is_same<
            tag_of_t<inherit_simple>,
            ext::std::integral_constant_tag<int>
        >{}, "");
        static_assert(std::is_same<
            tag_of_t<inherit_no_default>,
            ext::std::integral_constant_tag<int>
        >{}, "");
        static_assert(std::is_same<
            tag_of_t<std::is_pointer<int*>>,
            ext::std::integral_constant_tag<bool>
        >{}, "");

        static_assert(!std::is_same<
            tag_of_t<incomplete>,
            ext::std::integral_constant_tag<int>
        >{}, "");
        static_assert(!std::is_same<
            tag_of_t<empty_type>,
            ext::std::integral_constant_tag<int>
        >{}, "");
        static_assert(!std::is_same<
            tag_of_t<non_pod>,
            ext::std::integral_constant_tag<int>
        >{}, "");
        static_assert(!std::is_same<
            tag_of_t<void>,
            ext::std::integral_constant_tag<int>
        >{}, "");
    }

    //////////////////////////////////////////////////////////////////////////
    // Interoperation with hana::integral_constant
    //////////////////////////////////////////////////////////////////////////
    {
        BOOST_HANA_CONSTANT_CHECK(std::integral_constant<int, 1>{} == int_c<1>);
        BOOST_HANA_CONSTANT_CHECK(std::integral_constant<int, 1>{} == long_c<1>);

        BOOST_HANA_CONSTANT_CHECK(std::integral_constant<int, 2>{} != int_c<3>);
    }

#elif BOOST_HANA_TEST_PART == 2
    //////////////////////////////////////////////////////////////////////////
    // Constant
    //////////////////////////////////////////////////////////////////////////
    {
        // value
        static_assert(value(std::integral_constant<int, 0>{}) == 0, "");
        static_assert(value(std::integral_constant<int, 1>{}) == 1, "");
        static_assert(value(std::integral_constant<int, 3>{}) == 3, "");

        // laws
        test::TestConstant<ext::std::integral_constant_tag<int>>{ints, tuple_t<int, long, long long>};
    }

#elif BOOST_HANA_TEST_PART == 3
    //////////////////////////////////////////////////////////////////////////
    // Monoid, Group, Ring, EuclideanRing
    //////////////////////////////////////////////////////////////////////////
    {
        test::TestMonoid<ext::std::integral_constant_tag<int>>{ints};
        test::TestGroup<ext::std::integral_constant_tag<int>>{ints};
        test::TestRing<ext::std::integral_constant_tag<int>>{ints};
        test::TestEuclideanRing<ext::std::integral_constant_tag<int>>{ints};
    }

#elif BOOST_HANA_TEST_PART == 4
    //////////////////////////////////////////////////////////////////////////
    // Logical
    //////////////////////////////////////////////////////////////////////////
    {
        auto t = test::ct_eq<3>{};
        auto e = test::ct_eq<4>{};

        // eval_if
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                eval_if(std::true_type{}, always(t), always(e)),
                t
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                eval_if(std::false_type{}, always(t), always(e)),
                e
            ));
        }

        // not_
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                not_(std::true_type{}),
                std::false_type{}
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                not_(std::false_type{}),
                std::true_type{}
            ));
        }

        auto ints = make<tuple_tag>(
            std::integral_constant<int, -2>{},
            std::integral_constant<int, 0>{},
            std::integral_constant<int, 1>{},
            std::integral_constant<int, 3>{}
        );

        auto bools = make<tuple_tag>(std::true_type{}, std::false_type{});

        // laws
        test::TestLogical<ext::std::integral_constant_tag<int>>{ints};
        test::TestLogical<ext::std::integral_constant_tag<bool>>{bools};
    }

#elif BOOST_HANA_TEST_PART == 5
    //////////////////////////////////////////////////////////////////////////
    // Comparable and Hashable
    //////////////////////////////////////////////////////////////////////////
    test::TestComparable<ext::std::integral_constant_tag<int>>{ints};
    test::TestHashable<ext::std::integral_constant_tag<void>>{ints};

#elif BOOST_HANA_TEST_PART == 6
    //////////////////////////////////////////////////////////////////////////
    // Orderable
    //////////////////////////////////////////////////////////////////////////
    test::TestOrderable<ext::std::integral_constant_tag<int>>{ints};
#endif
}
