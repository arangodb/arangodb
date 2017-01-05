// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_CARTESIAN_PRODUCT_HPP
#define BOOST_HANA_TEST_AUTO_CARTESIAN_PRODUCT_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/cartesian_product.hpp>
#include <boost/hana/equal.hpp>

#include <laws/base.hpp>
#include "test_case.hpp"


TestCase test_cartesian_product{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    // 0 lists
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::cartesian_product(MAKE_TUPLE()),
        MAKE_TUPLE()
    ));

    // 1 list
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE()
        )),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<0>{})
        )),
        MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<0>{})
        )
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})
        )),
        MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<0>{}),
            MAKE_TUPLE(ct_eq<1>{})
        )
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
        )),
        MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<0>{}),
            MAKE_TUPLE(ct_eq<1>{}),
            MAKE_TUPLE(ct_eq<2>{})
        )
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        )),
        MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<0>{}),
            MAKE_TUPLE(ct_eq<1>{}),
            MAKE_TUPLE(ct_eq<2>{}),
            MAKE_TUPLE(ct_eq<3>{})
        )
    ));

    // 2 lists
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(),
            MAKE_TUPLE()
        )),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<00>{}),
            MAKE_TUPLE()
        )),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<00>{}),
            MAKE_TUPLE(ct_eq<10>{})
        )),
        MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<00>{}, ct_eq<10>{})
        )
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<00>{}, ct_eq<01>{}),
            MAKE_TUPLE(ct_eq<10>{})
        )),
        MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<00>{}, ct_eq<10>{}),
            MAKE_TUPLE(ct_eq<01>{}, ct_eq<10>{})
        )
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<00>{}),
            MAKE_TUPLE(ct_eq<10>{}, ct_eq<11>{})
        )),
        MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<00>{}, ct_eq<10>{}),
            MAKE_TUPLE(ct_eq<00>{}, ct_eq<11>{})
        )
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<00>{}, ct_eq<01>{}),
            MAKE_TUPLE(ct_eq<10>{}, ct_eq<11>{})
        )),
        MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<00>{}, ct_eq<10>{}),
            MAKE_TUPLE(ct_eq<00>{}, ct_eq<11>{}),
            MAKE_TUPLE(ct_eq<01>{}, ct_eq<10>{}),
            MAKE_TUPLE(ct_eq<01>{}, ct_eq<11>{})
        )
    ));

    // misc
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<00>{}),
            MAKE_TUPLE(ct_eq<10>{}),
            MAKE_TUPLE(ct_eq<20>{}),
            MAKE_TUPLE(ct_eq<30>{}, ct_eq<31>{})
        )),
        MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<00>{}, ct_eq<10>{}, ct_eq<20>{}, ct_eq<30>{}),
            MAKE_TUPLE(ct_eq<00>{}, ct_eq<10>{}, ct_eq<20>{}, ct_eq<31>{})
        )
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<00>{}),
            MAKE_TUPLE(ct_eq<10>{}),
            MAKE_TUPLE(ct_eq<20>{}, ct_eq<21>{}),
            MAKE_TUPLE(ct_eq<30>{}, ct_eq<31>{})
        )),
        MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<00>{}, ct_eq<10>{}, ct_eq<20>{}, ct_eq<30>{}),
            MAKE_TUPLE(ct_eq<00>{}, ct_eq<10>{}, ct_eq<20>{}, ct_eq<31>{}),
            MAKE_TUPLE(ct_eq<00>{}, ct_eq<10>{}, ct_eq<21>{}, ct_eq<30>{}),
            MAKE_TUPLE(ct_eq<00>{}, ct_eq<10>{}, ct_eq<21>{}, ct_eq<31>{})
        )
    ));


    // cartesian_product in a constexpr context
#ifndef MAKE_TUPLE_NO_CONSTEXPR
    static_assert(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(1),
            MAKE_TUPLE('a', 'b')
        )),
        MAKE_TUPLE(
            MAKE_TUPLE(1, 'a'),
            MAKE_TUPLE(1, 'b')
        )
    ), "");

    static_assert(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(1, 2),
            MAKE_TUPLE('a')
        )),
        MAKE_TUPLE(
            MAKE_TUPLE(1, 'a'),
            MAKE_TUPLE(2, 'a')
        )
    ), "");

    static_assert(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(1, 2),
            MAKE_TUPLE('a', 'b')
        )),
        MAKE_TUPLE(
            MAKE_TUPLE(1, 'a'),
            MAKE_TUPLE(1, 'b'),
            MAKE_TUPLE(2, 'a'),
            MAKE_TUPLE(2, 'b')
        )
    ), "");

    static_assert(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(1),
            MAKE_TUPLE('a'),
            MAKE_TUPLE(1.f),
            MAKE_TUPLE(1l, 2l)
        )),
        MAKE_TUPLE(
            MAKE_TUPLE(1, 'a', 1.f, 1l),
            MAKE_TUPLE(1, 'a', 1.f, 2l)
        )
    ), "");

    static_assert(hana::equal(
        hana::cartesian_product(MAKE_TUPLE(
            MAKE_TUPLE(1),
            MAKE_TUPLE('a'),
            MAKE_TUPLE(1.f),
            MAKE_TUPLE(1l, 2l),
            MAKE_TUPLE(nullptr)
        )),
        MAKE_TUPLE(
            MAKE_TUPLE(1, 'a', 1.f, 1l, nullptr),
            MAKE_TUPLE(1, 'a', 1.f, 2l, nullptr)
        )
    ), "");
#endif
}};

#endif // !BOOST_HANA_TEST_AUTO_CARTESIAN_PRODUCT_HPP
