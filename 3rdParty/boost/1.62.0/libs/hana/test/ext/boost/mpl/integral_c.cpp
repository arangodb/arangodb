// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/boost/mpl/integral_c.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/core/tag_of.hpp>
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

#include <boost/mpl/bool.hpp>
#include <boost/mpl/char.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/integral_c.hpp>
#include <boost/mpl/long.hpp>
#include <boost/mpl/size_t.hpp>

#include <cstddef>
#include <type_traits>
using namespace boost::hana;
namespace mpl = boost::mpl;


int main() {
    auto ints = make<tuple_tag>(
        mpl::int_<-10>{}, mpl::int_<-2>{}, mpl::integral_c<int, 0>{},
        mpl::integral_c<int, 1>{}, mpl::integral_c<int, 3>{}
    );
    (void)ints;

#if BOOST_HANA_TEST_PART == 1
    //////////////////////////////////////////////////////////////////////////
    // Make sure the tag is correct
    //////////////////////////////////////////////////////////////////////////
    {
        static_assert(std::is_same<
            tag_of_t<mpl::bool_<true>>,
            ext::boost::mpl::integral_c_tag<bool>
        >::value, "");

        static_assert(std::is_same<
            tag_of_t<mpl::int_<0>>,
            ext::boost::mpl::integral_c_tag<int>
        >::value, "");

        static_assert(std::is_same<
            tag_of_t<mpl::long_<0>>,
            ext::boost::mpl::integral_c_tag<long>
        >::value, "");

        static_assert(std::is_same<
            tag_of_t<mpl::size_t<0>>,
            ext::boost::mpl::integral_c_tag<std::size_t>
        >::value, "");

        static_assert(std::is_same<
            tag_of_t<mpl::integral_c<int, 0>>,
            ext::boost::mpl::integral_c_tag<int>
        >::value, "");

        static_assert(std::is_same<
            tag_of_t<mpl::char_<0>>,
            ext::boost::mpl::integral_c_tag<char>
        >::value, "");
    }

    //////////////////////////////////////////////////////////////////////////
    // Interoperation with hana::integral_constant
    //////////////////////////////////////////////////////////////////////////
    {
        BOOST_HANA_CONSTANT_CHECK(mpl::integral_c<int, 1>{} == int_c<1>);
        BOOST_HANA_CONSTANT_CHECK(mpl::integral_c<int, 1>{} == long_c<1>);

        BOOST_HANA_CONSTANT_CHECK(mpl::integral_c<int, 2>{} != int_c<3>);
    }

    //////////////////////////////////////////////////////////////////////////
    // Constant
    //////////////////////////////////////////////////////////////////////////
    {
        // value
        static_assert(value(mpl::integral_c<int, 0>{}) == 0, "");
        static_assert(value(mpl::integral_c<int, 1>{}) == 1, "");
        static_assert(value(mpl::integral_c<int, 3>{}) == 3, "");

        // laws
        test::TestConstant<ext::boost::mpl::integral_c_tag<int>>{ints, tuple_t<int, long, long long>};
    }

#elif BOOST_HANA_TEST_PART == 2
    //////////////////////////////////////////////////////////////////////////
    // Monoid, Group, Ring, EuclideanRing
    //////////////////////////////////////////////////////////////////////////
    test::TestMonoid<ext::boost::mpl::integral_c_tag<int>>{ints};
    test::TestGroup<ext::boost::mpl::integral_c_tag<int>>{ints};

#elif BOOST_HANA_TEST_PART == 3
    test::TestRing<ext::boost::mpl::integral_c_tag<int>>{ints};
    test::TestEuclideanRing<ext::boost::mpl::integral_c_tag<int>>{ints};

#elif BOOST_HANA_TEST_PART == 4 || BOOST_HANA_TEST_PART == 5
    //////////////////////////////////////////////////////////////////////////
    // Logical
    //////////////////////////////////////////////////////////////////////////
    {
        auto t = test::ct_eq<3>{};
        auto e = test::ct_eq<4>{};

        // eval_if
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                eval_if(mpl::true_{}, always(t), always(e)),
                t
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                eval_if(mpl::false_{}, always(t), always(e)),
                e
            ));
        }

        // not_
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                not_(mpl::true_{}),
                mpl::false_{}
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                not_(mpl::false_{}),
                mpl::true_{}
            ));
        }

        // laws
#if BOOST_HANA_TEST_PART == 4

        auto ints = make<tuple_tag>(
            mpl::int_<-2>{}, mpl::integral_c<int, 0>{}, mpl::integral_c<int, 3>{}
        );
        test::TestLogical<ext::boost::mpl::integral_c_tag<int>>{ints};

#elif BOOST_HANA_TEST_PART == 5

        auto bools = make<tuple_tag>(
            mpl::true_{}, mpl::false_{},
            mpl::integral_c<bool, true>{}, mpl::integral_c<bool, false>{}
        );
        test::TestLogical<ext::boost::mpl::integral_c_tag<bool>>{bools};

#endif
    }

#elif BOOST_HANA_TEST_PART == 6
    //////////////////////////////////////////////////////////////////////////
    // Comparable
    //////////////////////////////////////////////////////////////////////////
    test::TestComparable<ext::boost::mpl::integral_c_tag<int>>{ints};
    test::TestHashable<ext::boost::mpl::integral_c_tag<void>>{ints};

#elif BOOST_HANA_TEST_PART == 7
    //////////////////////////////////////////////////////////////////////////
    // Orderable
    //////////////////////////////////////////////////////////////////////////
    test::TestOrderable<ext::boost::mpl::integral_c_tag<int>>{ints};
#endif
}
