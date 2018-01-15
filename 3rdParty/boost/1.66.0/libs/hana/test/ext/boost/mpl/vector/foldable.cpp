// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/boost/mpl/vector.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/unpack.hpp>
#include <boost/hana/type.hpp>

#include <laws/base.hpp>
#include <laws/foldable.hpp>

#include <boost/mpl/vector.hpp>
namespace hana = boost::hana;
namespace mpl = boost::mpl;


struct t1; struct t2; struct t3; struct t4;

int main() {
    // unpack
    {
        hana::test::_injection<0> f{};
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(mpl::vector<>{}, f),
            f()
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(mpl::vector<t1>{}, f),
            f(hana::type_c<t1>)
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(mpl::vector<t1, t2>{}, f),
            f(hana::type_c<t1>, hana::type_c<t2>)
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(mpl::vector<t1, t2, t3>{}, f),
            f(hana::type_c<t1>, hana::type_c<t2>, hana::type_c<t3>)
        ));
    }

    // laws
    auto vectors = hana::make_tuple(
          mpl::vector<>{}
        , mpl::vector<t1>{}
        , mpl::vector<t1, t2>{}
        , mpl::vector<t1, t2, t3>{}
        , mpl::vector<t1, t2, t3, t4>{}
    );

    hana::test::TestFoldable<hana::ext::boost::mpl::vector_tag>{vectors};
}
