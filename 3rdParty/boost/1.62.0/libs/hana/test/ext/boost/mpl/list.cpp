// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/ext/boost/mpl/list.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <laws/base.hpp>
#include <laws/comparable.hpp>
#include <laws/foldable.hpp>
#include <laws/iterable.hpp>
#include <laws/searchable.hpp>
#include <support/seq.hpp>

#include <boost/mpl/identity.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/quote.hpp>
#include <boost/mpl/transform.hpp>

#include <type_traits>
using namespace boost::hana;
namespace mpl = boost::mpl;


struct t1; struct t2; struct t3; struct t4;

int main() {
    //////////////////////////////////////////////////////////////////////////
    // Setup for the laws below
    //////////////////////////////////////////////////////////////////////////
    auto lists = make<tuple_tag>(
          mpl::list<>{}
        , mpl::list<t1>{}
        , mpl::list<t1, t2>{}
        , mpl::list<t1, t2, t3>{}
        , mpl::list<t1, t2, t3, t4>{}
    );

#if BOOST_HANA_TEST_PART == 1
    //////////////////////////////////////////////////////////////////////////
    // Make sure the tag is resolved correctly
    //////////////////////////////////////////////////////////////////////////
    {
        using mpl_id = mpl::quote1<mpl::identity>;

        static_assert(std::is_same<
            tag_of_t<mpl::list<>>,
            ext::boost::mpl::list_tag
        >{}, "");
        static_assert(std::is_same<
            tag_of_t<mpl::list<>::type>,
            ext::boost::mpl::list_tag
        >{}, "");
        static_assert(std::is_same<
            tag_of_t<mpl::transform<mpl::list<>, mpl_id>::type>,
            ext::boost::mpl::list_tag
        >{}, "");

        static_assert(std::is_same<
            tag_of_t<mpl::list<t1>>,
            ext::boost::mpl::list_tag
        >{}, "");
        static_assert(std::is_same<
            tag_of_t<mpl::list<t1>::type>,
            ext::boost::mpl::list_tag
        >{}, "");
        static_assert(std::is_same<
            tag_of_t<mpl::transform<mpl::list<t1>, mpl_id>::type>,
            ext::boost::mpl::list_tag
        >{}, "");

        static_assert(std::is_same<
            tag_of_t<mpl::list<t1, t2>>,
            ext::boost::mpl::list_tag
        >{}, "");
        static_assert(std::is_same<
            tag_of_t<mpl::list<t1, t2>::type>,
            ext::boost::mpl::list_tag
        >{}, "");
        static_assert(std::is_same<
            tag_of_t<mpl::transform<mpl::list<t1, t2>, mpl_id>::type>,
            ext::boost::mpl::list_tag
        >{}, "");
    }

    //////////////////////////////////////////////////////////////////////////
    // Conversion from any `Foldable` containing `type`s
    //////////////////////////////////////////////////////////////////////////
    {
        auto foldable = ::seq;
        auto to_list = to<ext::boost::mpl::list_tag>;
        BOOST_HANA_CONSTANT_CHECK(equal(
            to_list(foldable()),
            mpl::list<>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            to_list(foldable(type_c<t1>)),
            mpl::list<t1>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            to_list(foldable(type_c<t1>, type_c<t2>)),
            mpl::list<t1, t2>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            to_list(foldable(type_c<t1>, type_c<t2>, type_c<t3>)),
            mpl::list<t1, t2, t3>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            to_list(foldable(type_c<t1>, type_c<t2>, type_c<t3>, type_c<t4>)),
            mpl::list<t1, t2, t3, t4>{}
        ));
    }

    //////////////////////////////////////////////////////////////////////////
    // Comparable
    //////////////////////////////////////////////////////////////////////////
    test::TestComparable<ext::boost::mpl::list_tag>{lists};

#elif BOOST_HANA_TEST_PART == 2
    //////////////////////////////////////////////////////////////////////////
    // Foldable
    //////////////////////////////////////////////////////////////////////////
    {
        // unpack
        {
            test::_injection<0> f{};
            BOOST_HANA_CONSTANT_CHECK(equal(
                unpack(mpl::list<>{}, f),
                f()
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                unpack(mpl::list<t1>{}, f),
                f(type_c<t1>)
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                unpack(mpl::list<t1, t2>{}, f),
                f(type_c<t1>, type_c<t2>)
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                unpack(mpl::list<t1, t2, t3>{}, f),
                f(type_c<t1>, type_c<t2>, type_c<t3>)
            ));
        }

        // laws
        test::TestFoldable<ext::boost::mpl::list_tag>{lists};
    }

#elif BOOST_HANA_TEST_PART == 3
    //////////////////////////////////////////////////////////////////////////
    // Iterable
    //////////////////////////////////////////////////////////////////////////
    {
        // front
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                front(mpl::list<t1>{}),
                type_c<t1>
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                front(mpl::list<t1, t2>{}),
                type_c<t1>
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                front(mpl::list<t1, t2, t3>{}),
                type_c<t1>
            ));
        }

        // drop_front_exactly
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                drop_front_exactly(mpl::list<t1>{}),
                mpl::list<>{}
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                drop_front_exactly(mpl::list<t1, t2>{}),
                mpl::list<t2>{}
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                drop_front_exactly(mpl::list<t1, t2, t3>{}),
                mpl::list<t2, t3>{}
            ));


            BOOST_HANA_CONSTANT_CHECK(equal(
                drop_front_exactly(mpl::list<t1, t2, t3>{}, size_c<2>),
                mpl::list<t3>{}
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                drop_front_exactly(mpl::list<t1, t2, t3, t4>{}, size_c<2>),
                mpl::list<t3, t4>{}
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                drop_front_exactly(mpl::list<t1, t2, t3, t4>{}, size_c<3>),
                mpl::list<t4>{}
            ));
        }

        // is_empty
        {
            BOOST_HANA_CONSTANT_CHECK(is_empty(mpl::list<>{}));
            BOOST_HANA_CONSTANT_CHECK(is_empty(mpl::list0<>{}));

            BOOST_HANA_CONSTANT_CHECK(not_(is_empty(mpl::list<t1>{})));
            BOOST_HANA_CONSTANT_CHECK(not_(is_empty(mpl::list1<t1>{})));

            BOOST_HANA_CONSTANT_CHECK(not_(is_empty(mpl::list<t1, t2>{})));
            BOOST_HANA_CONSTANT_CHECK(not_(is_empty(mpl::list2<t1, t2>{})));
        }

        // laws
        test::TestIterable<ext::boost::mpl::list_tag>{lists};
    }

#elif BOOST_HANA_TEST_PART == 4
    //////////////////////////////////////////////////////////////////////////
    // Searchable
    //////////////////////////////////////////////////////////////////////////
    {
        auto keys = tuple_t<t1, t2, void>;

        test::TestSearchable<ext::boost::mpl::list_tag>{lists, keys};
    }
#endif
}
