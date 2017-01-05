// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ap.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/experimental/view.hpp>
#include <boost/hana/functional/id.hpp>

#include <laws/base.hpp>
#include <support/seq.hpp>
namespace hana = boost::hana;
using hana::test::_injection;
using hana::test::ct_eq;


int main() {
    auto container = ::seq;
    auto f = hana::test::_injection<99>{};

    {
        auto storage = container();
        auto functions = container();
        auto transformed_storage = hana::experimental::transformed(storage, f);
        auto transformed_functions = hana::experimental::transformed(functions, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::ap(transformed_functions, transformed_storage),
            container()
        ));
    }{
        auto storage = container(ct_eq<0>{});
        auto functions = container();
        auto transformed_storage = hana::experimental::transformed(storage, f);
        auto transformed_functions = hana::experimental::transformed(functions, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::ap(transformed_functions, transformed_storage),
            container()
        ));
    }{
        auto storage = container();
        auto functions = container(ct_eq<0>{});
        auto transformed_storage = hana::experimental::transformed(storage, f);
        auto transformed_functions = hana::experimental::transformed(functions, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::ap(transformed_functions, transformed_storage),
            container()
        ));
    }

    {
        auto storage = container(ct_eq<0>{});
        auto functions = container(_injection<0>{});
        auto transformed_storage = hana::experimental::transformed(storage, f);
        auto transformed_functions = hana::experimental::transformed(functions, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::ap(transformed_functions, transformed_storage),
            container(_injection<0>{}(f(ct_eq<0>{})))
        ));
    }{
        auto storage = container(ct_eq<0>{});
        auto functions = container(_injection<0>{}, _injection<1>{});
        auto transformed_storage = hana::experimental::transformed(storage, f);
        auto transformed_functions = hana::experimental::transformed(functions, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::ap(transformed_functions, transformed_storage),
            container(_injection<0>{}(f(ct_eq<0>{})),
                      _injection<1>{}(f(ct_eq<0>{})))
        ));
    }{
        auto storage = container(ct_eq<0>{});
        auto functions = container(_injection<0>{}, _injection<1>{}, _injection<2>{});
        auto transformed_storage = hana::experimental::transformed(storage, f);
        auto transformed_functions = hana::experimental::transformed(functions, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::ap(transformed_functions, transformed_storage),
            container(_injection<0>{}(f(ct_eq<0>{})),
                      _injection<1>{}(f(ct_eq<0>{})),
                      _injection<2>{}(f(ct_eq<0>{})))
        ));
    }

    {
        auto storage = container(ct_eq<0>{}, ct_eq<1>{});
        auto functions = container(_injection<0>{});
        auto transformed_storage = hana::experimental::transformed(storage, f);
        auto transformed_functions = hana::experimental::transformed(functions, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::ap(transformed_functions, transformed_storage),
            container(_injection<0>{}(f(ct_eq<0>{})),
                      _injection<0>{}(f(ct_eq<1>{})))
        ));
    }{
        auto storage = container(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
        auto functions = container(_injection<0>{});
        auto transformed_storage = hana::experimental::transformed(storage, f);
        auto transformed_functions = hana::experimental::transformed(functions, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::ap(transformed_functions, transformed_storage),
            container(_injection<0>{}(f(ct_eq<0>{})),
                      _injection<0>{}(f(ct_eq<1>{})),
                      _injection<0>{}(f(ct_eq<2>{})))
        ));
    }

    {
        auto storage = container(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
        auto functions = container(_injection<0>{}, _injection<1>{});
        auto transformed_storage = hana::experimental::transformed(storage, f);
        auto transformed_functions = hana::experimental::transformed(functions, hana::id);
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::ap(transformed_functions, transformed_storage),
            container(_injection<0>{}(f(ct_eq<0>{})),
                      _injection<0>{}(f(ct_eq<1>{})),
                      _injection<0>{}(f(ct_eq<2>{})),

                      _injection<1>{}(f(ct_eq<0>{})),
                      _injection<1>{}(f(ct_eq<1>{})),
                      _injection<1>{}(f(ct_eq<2>{})))
        ));
    }
}
