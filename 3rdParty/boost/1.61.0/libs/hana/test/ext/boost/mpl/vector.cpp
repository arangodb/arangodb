// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/ext/boost/mpl/vector.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <laws/base.hpp>
#include <laws/comparable.hpp>
#include <laws/foldable.hpp>
#include <laws/iterable.hpp>
#include <laws/searchable.hpp>
#include <support/seq.hpp>

#include <boost/mpl/identity.hpp>
#include <boost/mpl/quote.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/vector.hpp>

#include <type_traits>
using namespace boost::hana;
namespace mpl = boost::mpl;


struct t1; struct t2; struct t3; struct t4;

int main() {
    //////////////////////////////////////////////////////////////////////////
    // Setup for the laws below
    //////////////////////////////////////////////////////////////////////////
    auto vectors = make<tuple_tag>(
          mpl::vector<>{}
        , mpl::vector<t1>{}
        , mpl::vector<t1, t2>{}
        , mpl::vector<t1, t2, t3>{}
        , mpl::vector<t1, t2, t3, t4>{}
    );

#if BOOST_HANA_TEST_PART == 1
    //////////////////////////////////////////////////////////////////////////
    // Make sure the tag is resolved correctly
    //////////////////////////////////////////////////////////////////////////
    {
        using mpl_id = mpl::quote1<mpl::identity>;

        static_assert(std::is_same<
            tag_of_t<mpl::vector<>>,
            ext::boost::mpl::vector_tag
        >{}, "");
        static_assert(std::is_same<
            tag_of_t<mpl::vector<>::type>,
            ext::boost::mpl::vector_tag
        >{}, "");
        static_assert(std::is_same<
            tag_of_t<mpl::transform<mpl::vector<>, mpl_id>::type>,
            ext::boost::mpl::vector_tag
        >{}, "");

        static_assert(std::is_same<
            tag_of_t<mpl::vector<t1>>,
            ext::boost::mpl::vector_tag
        >{}, "");
        static_assert(std::is_same<
            tag_of_t<mpl::vector<t1>::type>,
            ext::boost::mpl::vector_tag
        >{}, "");
        static_assert(std::is_same<
            tag_of_t<mpl::transform<mpl::vector<t1>, mpl_id>::type>,
            ext::boost::mpl::vector_tag
        >{}, "");

        static_assert(std::is_same<
            tag_of_t<mpl::vector<t1, t2>>,
            ext::boost::mpl::vector_tag
        >{}, "");
        static_assert(std::is_same<
            tag_of_t<mpl::vector<t1, t2>::type>,
            ext::boost::mpl::vector_tag
        >{}, "");
        static_assert(std::is_same<
            tag_of_t<mpl::transform<mpl::vector<t1, t2>, mpl_id>::type>,
            ext::boost::mpl::vector_tag
        >{}, "");
    }

    //////////////////////////////////////////////////////////////////////////
    // Conversion from any `Foldable` containing `type`s
    //////////////////////////////////////////////////////////////////////////
    {
        auto foldable = ::seq;
        auto to_vec = to<ext::boost::mpl::vector_tag>;
        BOOST_HANA_CONSTANT_CHECK(equal(
            to_vec(foldable()),
            mpl::vector<>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            to_vec(foldable(type_c<t1>)),
            mpl::vector<t1>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            to_vec(foldable(type_c<t1>, type_c<t2>)),
            mpl::vector<t1, t2>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            to_vec(foldable(type_c<t1>, type_c<t2>, type_c<t3>)),
            mpl::vector<t1, t2, t3>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(equal(
            to_vec(foldable(type_c<t1>, type_c<t2>, type_c<t3>, type_c<t4>)),
            mpl::vector<t1, t2, t3, t4>{}
        ));
    }

    //////////////////////////////////////////////////////////////////////////
    // Comparable
    //////////////////////////////////////////////////////////////////////////
    test::TestComparable<ext::boost::mpl::vector_tag>{vectors};

#elif BOOST_HANA_TEST_PART == 2
    //////////////////////////////////////////////////////////////////////////
    // Foldable
    //////////////////////////////////////////////////////////////////////////
    {
        // unpack
        {
            test::_injection<0> f{};
            BOOST_HANA_CONSTANT_CHECK(equal(
                unpack(mpl::vector<>{}, f),
                f()
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                unpack(mpl::vector<t1>{}, f),
                f(type_c<t1>)
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                unpack(mpl::vector<t1, t2>{}, f),
                f(type_c<t1>, type_c<t2>)
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                unpack(mpl::vector<t1, t2, t3>{}, f),
                f(type_c<t1>, type_c<t2>, type_c<t3>)
            ));
        }

        // laws
        test::TestFoldable<ext::boost::mpl::vector_tag>{vectors};
    }

#elif BOOST_HANA_TEST_PART == 3
    //////////////////////////////////////////////////////////////////////////
    // Iterable
    //////////////////////////////////////////////////////////////////////////
    {
        // front
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                front(mpl::vector<t1>{}),
                type_c<t1>
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                front(mpl::vector<t1, t2>{}),
                type_c<t1>
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                front(mpl::vector<t1, t2, t3>{}),
                type_c<t1>
            ));
        }

        // drop_front_exactly
        {
            BOOST_HANA_CONSTANT_CHECK(equal(
                drop_front_exactly(mpl::vector<t1>{}),
                mpl::vector<>{}
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                drop_front_exactly(mpl::vector<t1, t2>{}),
                mpl::vector<t2>{}
            ));
            BOOST_HANA_CONSTANT_CHECK(equal(
                drop_front_exactly(mpl::vector<t1, t2, t3>{}),
                mpl::vector<t2, t3>{}
            ));


            BOOST_HANA_CONSTANT_CHECK(equal(
                drop_front_exactly(mpl::vector<t1, t2, t3>{}, size_c<2>),
                mpl::vector<t3>{}
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                drop_front_exactly(mpl::vector<t1, t2, t3, t4>{}, size_c<2>),
                mpl::vector<t3, t4>{}
            ));

            BOOST_HANA_CONSTANT_CHECK(equal(
                drop_front_exactly(mpl::vector<t1, t2, t3, t4>{}, size_c<3>),
                mpl::vector<t4>{}
            ));
        }

        // is_empty
        {
            BOOST_HANA_CONSTANT_CHECK(is_empty(mpl::vector<>{}));
            BOOST_HANA_CONSTANT_CHECK(is_empty(mpl::vector0<>{}));

            BOOST_HANA_CONSTANT_CHECK(not_(is_empty(mpl::vector<t1>{})));
            BOOST_HANA_CONSTANT_CHECK(not_(is_empty(mpl::vector1<t1>{})));

            BOOST_HANA_CONSTANT_CHECK(not_(is_empty(mpl::vector<t1, t2>{})));
            BOOST_HANA_CONSTANT_CHECK(not_(is_empty(mpl::vector2<t1, t2>{})));
        }

        // laws
        test::TestIterable<ext::boost::mpl::vector_tag>{vectors};
    }

#elif BOOST_HANA_TEST_PART == 4
    //////////////////////////////////////////////////////////////////////////
    // Searchable
    //////////////////////////////////////////////////////////////////////////
    {
        auto keys = tuple_t<t1, t2, void>;

        test::TestSearchable<ext::boost::mpl::vector_tag>{vectors, keys};
    }
#endif
}
