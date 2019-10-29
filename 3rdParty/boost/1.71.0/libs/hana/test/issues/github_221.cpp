// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/adjust_if.hpp>
#include <boost/hana/all_of.hpp>
#include <boost/hana/any_of.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/count_if.hpp>
#include <boost/hana/drop_while.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/filter.hpp>
#include <boost/hana/find_if.hpp>
#include <boost/hana/functional/id.hpp>
#include <boost/hana/functional/partial.hpp>
#include <boost/hana/group.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/lexicographical_compare.hpp>
#include <boost/hana/maximum.hpp>
#include <boost/hana/minimum.hpp>
#include <boost/hana/none_of.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/partition.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/remove_if.hpp>
#include <boost/hana/replace_if.hpp>
#include <boost/hana/sort.hpp>
#include <boost/hana/span.hpp>
#include <boost/hana/take_while.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/unique.hpp>
#include <boost/hana/while.hpp>
namespace hana = boost::hana;


constexpr auto increment = hana::partial(hana::plus, hana::int_c<1>);

int main() {

    // adjust_if
    {
        constexpr auto x = hana::adjust_if(hana::make_tuple(hana::int_c<0>), hana::id, increment);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::make_tuple(hana::int_c<0>)));

        constexpr auto y = hana::adjust_if(hana::make_tuple(hana::int_c<1>), hana::id, increment);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(y, hana::make_tuple(hana::int_c<2>)));

        constexpr auto z = hana::adjust_if(hana::make_tuple(hana::int_c<3>), hana::id, increment);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(z, hana::make_tuple(hana::int_c<4>)));

        constexpr auto l = hana::adjust_if(hana::tuple_c<int, 3>, hana::id, increment);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(l, hana::make_tuple(hana::int_c<4>)));
    }
    {
        // test with lvalue
        constexpr auto x = hana::adjust_if(hana::tuple_c<int, 3>, hana::id, increment);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::make_tuple(hana::int_c<4>)));
    }

    // all_of
    {
        BOOST_HANA_CONSTANT_CHECK(!hana::all_of(hana::make_tuple(hana::int_c<0>), hana::id));
        BOOST_HANA_CONSTANT_CHECK(hana::all_of(hana::make_tuple(hana::int_c<1>), hana::id));
        BOOST_HANA_CONSTANT_CHECK(hana::all_of(hana::make_tuple(hana::int_c<3>), hana::id));
        // test with lvalue
        BOOST_HANA_CONSTANT_CHECK(hana::all_of(hana::tuple_c<int, 3>, hana::id));
    }

    // any_of
    {
        BOOST_HANA_CONSTANT_CHECK(!hana::any_of(hana::make_tuple(hana::int_c<0>), hana::id));
        BOOST_HANA_CONSTANT_CHECK(hana::any_of(hana::make_tuple(hana::int_c<1>), hana::id));
        BOOST_HANA_CONSTANT_CHECK(hana::any_of(hana::make_tuple(hana::int_c<3>), hana::id));
        // test with lvalue
        BOOST_HANA_CONSTANT_CHECK(hana::any_of(hana::tuple_c<int, 3>, hana::id));
    }

    // count_if
    {
        constexpr auto x = hana::count_if(hana::make_tuple(hana::int_c<0>), hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::size_c<0>));

        constexpr auto y = hana::count_if(hana::make_tuple(hana::int_c<1>), hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(y, hana::size_c<1>));

        constexpr auto z = hana::count_if(hana::make_tuple(hana::int_c<3>), hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(z, hana::size_c<1>));
    }
    {
        // test with lvalue
        constexpr auto x = hana::count_if(hana::tuple_c<int, 3>, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::size_c<1>));
    }

    // drop_while
    {
        constexpr auto x = hana::drop_while(
            hana::make_tuple(hana::int_c<0>), hana::id
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x,
                hana::make_tuple(hana::int_c<0>)
            )
        );
        constexpr auto y = hana::drop_while(
            hana::make_tuple(hana::int_c<1>, hana::int_c<3>, hana::int_c<0>), hana::id
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(y,
                hana::make_tuple(hana::int_c<0>)
            )
        );
    }
    {
        // test with lvalue
        constexpr auto x = hana::drop_while(
            hana::tuple_c<int, 1, 3, 0>,  hana::id
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x,
                hana::make_tuple(hana::int_c<0>)
            )
        );
    }

    // filter
    {
        constexpr auto x = hana::filter(hana::make_tuple(hana::int_c<0>), hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::make_tuple()));

        constexpr auto y = hana::filter(hana::make_tuple(hana::int_c<1>), hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(y, hana::make_tuple(hana::int_c<1>)));

        constexpr auto z = hana::filter(hana::make_tuple(hana::int_c<3>), hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(z, hana::make_tuple(hana::int_c<3>)));

    }
    {
        // test with lvalue
        constexpr auto x = hana::filter(hana::tuple_c<int, 3>, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::make_tuple(hana::int_c<3>)));
    }

    // find_if
    {
        constexpr auto x = hana::find_if(hana::make_tuple(hana::int_c<0>), hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::nothing));

        constexpr auto y = hana::find_if(hana::make_tuple(hana::int_c<1>), hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(y, hana::just(hana::int_c<1>)));

        constexpr auto z = hana::find_if(hana::make_tuple(hana::int_c<3>), hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(z, hana::just(hana::int_c<3>)));
    }
    {
        // test with lvalue
        constexpr auto z = hana::find_if(hana::tuple_c<int, 3>, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(z, hana::just(hana::int_c<3>)));
    }
    {
        // test with std::tuple (for default implementation of find_if)
        constexpr auto x = hana::find_if(std::make_tuple(hana::int_c<0>), hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::nothing));

        constexpr auto y = hana::find_if(std::make_tuple(hana::int_c<1>), hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(y, hana::just(hana::int_c<1>)));

        constexpr auto z = hana::find_if(std::make_tuple(hana::int_c<3>), hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(z, hana::just(hana::int_c<3>)));
    }
    {
        // test with lvalue
        constexpr auto seq = std::make_tuple(hana::int_c<3>);
        constexpr auto x = hana::find_if(seq, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::just(hana::int_c<3>)));
    }

    // group
    {
        constexpr auto x = hana::group(
            hana::make_tuple(
                hana::int_c<0>,
                hana::int_c<0>,
                hana::int_c<1>,
                hana::int_c<1>,
                hana::int_c<2>
            ),
            hana::plus
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x,
                hana::make_tuple(
                    hana::tuple_c<int, 0>,
                    hana::tuple_c<int, 0, 1, 1, 2>
                )
            )
        );
    }
    {
        // test with lvalue
        constexpr auto x = hana::group(hana::tuple_c<int, 0, 0, 1, 1, 2>, hana::plus);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x,
                hana::make_tuple(
                    hana::tuple_c<int, 0>,
                    hana::tuple_c<int, 0, 1, 1, 2>
                )
            )
        );
    }

    // lexicographical_compare
    {
        BOOST_HANA_CONSTANT_CHECK(
            hana::lexicographical_compare(
                hana::make_tuple(hana::int_c<0>, hana::int_c<0>),
                hana::make_tuple(hana::int_c<0>, hana::int_c<3>),
                hana::plus
            )
        );
    }
    {
        // test with lvalue
        BOOST_HANA_CONSTANT_CHECK(
            hana::lexicographical_compare(
                hana::tuple_c<int, 0, 0>,
                hana::tuple_c<int, 0, 3>,
                hana::plus
            )
        );
    }

    // maximum
    {
        constexpr auto x = hana::maximum(
            hana::make_tuple(
                hana::int_c<0>,
                hana::int_c<0>,
                hana::int_c<1>,
                hana::int_c<3>,
                hana::int_c<2>
            ),
            hana::plus
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::int_c<2>));
    }
    {
        // test with lvalue
        constexpr auto x = hana::maximum(hana::tuple_c<int, 0, 0, 1, 3, 2>, hana::plus);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::int_c<2>));
    }

    // minimum
    {
        constexpr auto x = hana::minimum(
            hana::make_tuple(
                hana::int_c<0>,
                hana::int_c<0>,
                hana::int_c<1>,
                hana::int_c<3>,
                hana::int_c<2>
            ),
            hana::plus
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::int_c<0>));
    }
    {
        // test with lvalue
        constexpr auto x = hana::minimum(hana::tuple_c<int, 0, 0, 1, 3, 2>, hana::plus);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::int_c<0>));
    }

    // none_of
    {
        BOOST_HANA_CONSTANT_CHECK(hana::none_of(hana::make_tuple(hana::int_c<0>), hana::id));
        BOOST_HANA_CONSTANT_CHECK(!hana::none_of(hana::make_tuple(hana::int_c<1>), hana::id));
        BOOST_HANA_CONSTANT_CHECK(!hana::none_of(hana::make_tuple(hana::int_c<3>), hana::id));
    }
    {
        // test with lvalue
        BOOST_HANA_CONSTANT_CHECK(hana::none_of(hana::tuple_c<int, 0>, hana::id));
        BOOST_HANA_CONSTANT_CHECK(!hana::none_of(hana::tuple_c<int, 1>, hana::id));
        BOOST_HANA_CONSTANT_CHECK(!hana::none_of(hana::tuple_c<int, 3>, hana::id));
    }

    // partition
    {
        constexpr auto x = hana::partition(
            hana::make_tuple(
                hana::int_c<0>,
                hana::int_c<1>,
                hana::int_c<3>
            ),
            hana::id
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x,
                hana::make_pair(
                    hana::tuple_c<int, 1, 3>,
                    hana::tuple_c<int, 0>
                )
            )
        );
    }
    {
        // test with lvalue
        constexpr auto x = hana::partition(hana::tuple_c<int, 0, 1, 3>, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x,
                hana::make_pair(
                    hana::tuple_c<int, 1, 3>,
                    hana::tuple_c<int, 0>
                )
            )
        );
    }

    // remove_if
    {
        constexpr auto x = hana::remove_if(hana::make_tuple(hana::int_c<0>), hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::make_tuple(hana::int_c<0>)));

        constexpr auto y = hana::remove_if(hana::make_tuple(hana::int_c<1>), hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(y, hana::make_tuple()));

        constexpr auto z = hana::remove_if(hana::make_tuple(hana::int_c<3>), hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(z, hana::make_tuple()));
    }
    {
        // test with lvalue
        constexpr auto z = hana::remove_if(hana::tuple_c<int, 3>, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(z, hana::make_tuple()));
    }

    // replace_if
    {
        constexpr auto x = hana::replace_if(
            hana::make_tuple(hana::int_c<0>),
            hana::id,
            hana::int_c<42>
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::make_tuple(hana::int_c<0>)));

        constexpr auto y = hana::replace_if(
            hana::make_tuple(hana::int_c<1>),
            hana::id,
            hana::int_c<42>
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(y, hana::make_tuple(hana::int_c<42>)));

        constexpr auto z = hana::replace_if(
            hana::make_tuple(hana::int_c<3>),
            hana::id,
            hana::int_c<42>
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(z, hana::make_tuple(hana::int_c<42>)));
    }
    {
        // test with lvalue
        constexpr auto z = hana::replace_if(
            hana::tuple_c<int, 3>,
            hana::id,
            hana::int_c<42>
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(z, hana::make_tuple(hana::int_c<42>)));
    }

    // sort
    {
        constexpr auto x = hana::sort(
            hana::make_tuple(
                hana::int_c<0>,
                hana::int_c<1>,
                hana::int_c<2>
            ),
            hana::plus);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::tuple_c<int, 2, 1, 0>));
    }
    {
        // test with lvalue
        constexpr auto x = hana::sort(hana::tuple_c<int, 0, 1, 2>, hana::plus);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::tuple_c<int, 2, 1, 0>));
    }
    // span
    {
        constexpr auto x = hana::span(
            hana::make_tuple(
                hana::int_c<2>,
                hana::int_c<1>,
                hana::int_c<0>
            ),
            hana::id
        );
        BOOST_HANA_CONSTANT_CHECK(
            hana::equal(x,
                hana::make_pair(
                    hana::tuple_c<int, 2, 1>,
                    hana::tuple_c<int, 0>
                )
            )
        );
    }
    {
        // test with an lvalue
        constexpr auto x = hana::span(hana::tuple_c<int, 2, 1, 0>, hana::id);
        BOOST_HANA_CONSTANT_CHECK(
            hana::equal(x,
                hana::make_pair(
                    hana::tuple_c<int, 2, 1>,
                    hana::tuple_c<int, 0>
                )
            )
        );
    }

    // take_while
    {
        constexpr auto x = hana::take_while(
            hana::make_tuple(
                hana::int_c<2>,
                hana::int_c<1>,
                hana::int_c<0>
            ),
            hana::id
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::tuple_c<int, 2, 1>));
    }
    {
        // test with lvalue
        constexpr auto x = hana::take_while(hana::tuple_c<int, 2, 1, 0>, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::tuple_c<int, 2, 1>));
    }

    // unique
    {
        constexpr auto x = hana::unique(
            hana::make_tuple(
                hana::int_c<0>,
                hana::int_c<0>,
                hana::int_c<1>,
                hana::int_c<2>
            ),
            hana::plus
        );
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::tuple_c<int, 0, 0>));
    }
    {
        // test with lvalue
        constexpr auto x = hana::unique(hana::tuple_c<int, 0, 0, 1, 2>, hana::plus);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::tuple_c<int, 0, 0>));
    }

    // while
    {
        constexpr auto x = hana::while_(hana::id, hana::int_c<-3>, increment);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(x, hana::int_c<0>));
    }
}
