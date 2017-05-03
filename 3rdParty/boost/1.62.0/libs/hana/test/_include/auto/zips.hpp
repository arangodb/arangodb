// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_ZIPS_HPP
#define BOOST_HANA_TEST_AUTO_ZIPS_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/zip_shortest_with.hpp>
#include <boost/hana/zip_with.hpp>

#include <laws/base.hpp>
#include "test_case.hpp"


TestCase test_zip_shortest_with{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    hana::test::_injection<0> f{};
    auto zip = hana::zip_shortest_with;
    struct undefined { };

    // zip 1
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(undefined{}, MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(f, MAKE_TUPLE(ct_eq<0>{})),
        MAKE_TUPLE(f(ct_eq<0>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(f, MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
        MAKE_TUPLE(f(ct_eq<0>{}), f(ct_eq<1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(f, MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        MAKE_TUPLE(f(ct_eq<0>{}), f(ct_eq<1>{}), f(ct_eq<2>{}))
    ));

    // zip 2
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(undefined{}, MAKE_TUPLE(), MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(undefined{}, MAKE_TUPLE(undefined{}), MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(undefined{}, MAKE_TUPLE(), MAKE_TUPLE(undefined{})),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(f, MAKE_TUPLE(ct_eq<1>{}), MAKE_TUPLE(ct_eq<-1>{})),
        MAKE_TUPLE(f(ct_eq<1>{}, ct_eq<-1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(f, MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}), MAKE_TUPLE(ct_eq<-1>{})),
        MAKE_TUPLE(f(ct_eq<1>{}, ct_eq<-1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(f, MAKE_TUPLE(ct_eq<1>{}), MAKE_TUPLE(ct_eq<-1>{}, ct_eq<-2>{})),
        MAKE_TUPLE(f(ct_eq<1>{}, ct_eq<-1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(f, MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}), MAKE_TUPLE(ct_eq<-1>{}, ct_eq<-2>{})),
        MAKE_TUPLE(f(ct_eq<1>{}, ct_eq<-1>{}), f(ct_eq<2>{}, ct_eq<-2>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(f, MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
               MAKE_TUPLE(ct_eq<-1>{}, ct_eq<-2>{}, ct_eq<-3>{})),
        MAKE_TUPLE(f(ct_eq<1>{}, ct_eq<-1>{}),
             f(ct_eq<2>{}, ct_eq<-2>{}),
             f(ct_eq<3>{}, ct_eq<-3>{}))
    ));

    // zip 3
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(undefined{}, MAKE_TUPLE(), MAKE_TUPLE(), MAKE_TUPLE()),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(undefined{}, MAKE_TUPLE(undefined{}), MAKE_TUPLE(), MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(undefined{}, MAKE_TUPLE(), MAKE_TUPLE(undefined{}), MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(undefined{}, MAKE_TUPLE(), MAKE_TUPLE(), MAKE_TUPLE(undefined{})),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(undefined{}, MAKE_TUPLE(), MAKE_TUPLE(undefined{}), MAKE_TUPLE(undefined{})),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(undefined{}, MAKE_TUPLE(undefined{}), MAKE_TUPLE(), MAKE_TUPLE(undefined{})),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(undefined{}, MAKE_TUPLE(undefined{}), MAKE_TUPLE(undefined{}), MAKE_TUPLE()),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(f, MAKE_TUPLE(ct_eq<0>{}), MAKE_TUPLE(ct_eq<1>{}), MAKE_TUPLE(ct_eq<2>{})),
        MAKE_TUPLE(f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}))
    ));

    // zip 4
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(f,
            MAKE_TUPLE(ct_eq<11>{}, ct_eq<12>{}, ct_eq<13>{}, ct_eq<14>{}),
            MAKE_TUPLE(ct_eq<21>{}, ct_eq<22>{}, ct_eq<23>{}),
            MAKE_TUPLE(ct_eq<31>{}, ct_eq<32>{}, ct_eq<33>{}, ct_eq<34>{}),
            MAKE_TUPLE(ct_eq<41>{}, ct_eq<42>{}, ct_eq<43>{}, ct_eq<44>{}, ct_eq<45>{})
        ),
        MAKE_TUPLE(
            f(ct_eq<11>{}, ct_eq<21>{}, ct_eq<31>{}, ct_eq<41>{}),
            f(ct_eq<12>{}, ct_eq<22>{}, ct_eq<32>{}, ct_eq<42>{}),
            f(ct_eq<13>{}, ct_eq<23>{}, ct_eq<33>{}, ct_eq<43>{})
        )
    ));

    // zip 5
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        zip(f,
            MAKE_TUPLE(ct_eq<11>{}, ct_eq<12>{}, ct_eq<13>{}, ct_eq<14>{}),
            MAKE_TUPLE(ct_eq<21>{}, ct_eq<22>{}, ct_eq<23>{}, ct_eq<24>{}, ct_eq<25>{}),
            MAKE_TUPLE(ct_eq<31>{}, ct_eq<32>{}, ct_eq<33>{}, ct_eq<34>{}),
            MAKE_TUPLE(ct_eq<41>{}, ct_eq<42>{}, ct_eq<43>{}, ct_eq<44>{}, ct_eq<45>{}, ct_eq<46>{}),
            MAKE_TUPLE(ct_eq<51>{}, ct_eq<52>{}, ct_eq<53>{}, ct_eq<54>{}, ct_eq<55>{})
        ),
        MAKE_TUPLE(
            f(ct_eq<11>{}, ct_eq<21>{}, ct_eq<31>{}, ct_eq<41>{}, ct_eq<51>{}),
            f(ct_eq<12>{}, ct_eq<22>{}, ct_eq<32>{}, ct_eq<42>{}, ct_eq<52>{}),
            f(ct_eq<13>{}, ct_eq<23>{}, ct_eq<33>{}, ct_eq<43>{}, ct_eq<53>{}),
            f(ct_eq<14>{}, ct_eq<24>{}, ct_eq<34>{}, ct_eq<44>{}, ct_eq<54>{})
        )
    ));
}};

TestCase test_zip_with{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    hana::test::_injection<0> f{};
    struct undefined { };

    // zip 1
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_with(undefined{}, MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_with(f, MAKE_TUPLE(ct_eq<0>{})),
        MAKE_TUPLE(f(ct_eq<0>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_with(f, MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
        MAKE_TUPLE(f(ct_eq<0>{}), f(ct_eq<1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_with(f, MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        MAKE_TUPLE(f(ct_eq<0>{}), f(ct_eq<1>{}), f(ct_eq<2>{}))
    ));

    // zip 2
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_with(undefined{}, MAKE_TUPLE(), MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_with(f, MAKE_TUPLE(ct_eq<1>{}), MAKE_TUPLE(ct_eq<-1>{})),
        MAKE_TUPLE(f(ct_eq<1>{}, ct_eq<-1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_with(f, MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}), MAKE_TUPLE(ct_eq<-1>{}, ct_eq<-2>{})),
        MAKE_TUPLE(f(ct_eq<1>{}, ct_eq<-1>{}), f(ct_eq<2>{}, ct_eq<-2>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_with(f,
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            MAKE_TUPLE(ct_eq<-1>{}, ct_eq<-2>{}, ct_eq<-3>{})),
        MAKE_TUPLE(
            f(ct_eq<1>{}, ct_eq<-1>{}),
            f(ct_eq<2>{}, ct_eq<-2>{}),
            f(ct_eq<3>{}, ct_eq<-3>{}))
    ));

    // zip 3
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_with(undefined{}, MAKE_TUPLE(), MAKE_TUPLE(), MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_with(f, MAKE_TUPLE(ct_eq<0>{}), MAKE_TUPLE(ct_eq<1>{}), MAKE_TUPLE(ct_eq<2>{})),
        MAKE_TUPLE(f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_with(f,
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}),
            MAKE_TUPLE(ct_eq<2>{}, ct_eq<3>{}),
            MAKE_TUPLE(ct_eq<4>{}, ct_eq<5>{})
        ),
        MAKE_TUPLE(
            f(ct_eq<0>{}, ct_eq<2>{}, ct_eq<4>{}),
            f(ct_eq<1>{}, ct_eq<3>{}, ct_eq<5>{})
        )
    ));

    // zip 4
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_with(f,
            MAKE_TUPLE(ct_eq<11>{}, ct_eq<12>{}, ct_eq<13>{}),
            MAKE_TUPLE(ct_eq<21>{}, ct_eq<22>{}, ct_eq<23>{}),
            MAKE_TUPLE(ct_eq<31>{}, ct_eq<32>{}, ct_eq<33>{}),
            MAKE_TUPLE(ct_eq<41>{}, ct_eq<42>{}, ct_eq<43>{})
        ),
        MAKE_TUPLE(
            f(ct_eq<11>{}, ct_eq<21>{}, ct_eq<31>{}, ct_eq<41>{}),
            f(ct_eq<12>{}, ct_eq<22>{}, ct_eq<32>{}, ct_eq<42>{}),
            f(ct_eq<13>{}, ct_eq<23>{}, ct_eq<33>{}, ct_eq<43>{})
        )
    ));

    // zip 5
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_with(f,
            MAKE_TUPLE(ct_eq<11>{}, ct_eq<12>{}, ct_eq<13>{}, ct_eq<14>{}),
            MAKE_TUPLE(ct_eq<21>{}, ct_eq<22>{}, ct_eq<23>{}, ct_eq<24>{}),
            MAKE_TUPLE(ct_eq<31>{}, ct_eq<32>{}, ct_eq<33>{}, ct_eq<34>{}),
            MAKE_TUPLE(ct_eq<41>{}, ct_eq<42>{}, ct_eq<43>{}, ct_eq<44>{}),
            MAKE_TUPLE(ct_eq<51>{}, ct_eq<52>{}, ct_eq<53>{}, ct_eq<54>{})
        ),
        MAKE_TUPLE(
            f(ct_eq<11>{}, ct_eq<21>{}, ct_eq<31>{}, ct_eq<41>{}, ct_eq<51>{}),
            f(ct_eq<12>{}, ct_eq<22>{}, ct_eq<32>{}, ct_eq<42>{}, ct_eq<52>{}),
            f(ct_eq<13>{}, ct_eq<23>{}, ct_eq<33>{}, ct_eq<43>{}, ct_eq<53>{}),
            f(ct_eq<14>{}, ct_eq<24>{}, ct_eq<34>{}, ct_eq<44>{}, ct_eq<54>{})
        )
    ));
}};

TestCase test_zip{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip(MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip(MAKE_TUPLE(ct_eq<0>{})),
        MAKE_TUPLE(MAKE_TUPLE(ct_eq<0>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
        MAKE_TUPLE(MAKE_TUPLE(ct_eq<0>{}), MAKE_TUPLE(ct_eq<1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        MAKE_TUPLE(MAKE_TUPLE(ct_eq<0>{}), MAKE_TUPLE(ct_eq<1>{}), MAKE_TUPLE(ct_eq<2>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip(MAKE_TUPLE(), MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip(MAKE_TUPLE(ct_eq<0>{}), MAKE_TUPLE(ct_eq<1>{})),
        MAKE_TUPLE(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip(MAKE_TUPLE(ct_eq<0>{}), MAKE_TUPLE(ct_eq<1>{}), MAKE_TUPLE(ct_eq<2>{})),
        MAKE_TUPLE(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip(MAKE_TUPLE(ct_eq<0>{}, ct_eq<3>{}),
                  MAKE_TUPLE(ct_eq<1>{}, ct_eq<4>{}),
                  MAKE_TUPLE(ct_eq<2>{}, ct_eq<5>{})),
        MAKE_TUPLE(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
                   MAKE_TUPLE(ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}))
    ));
}};

TestCase test_zip_shortest{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_shortest(MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_shortest(MAKE_TUPLE(ct_eq<0>{})),
        MAKE_TUPLE(MAKE_TUPLE(ct_eq<0>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_shortest(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
        MAKE_TUPLE(MAKE_TUPLE(ct_eq<0>{}), MAKE_TUPLE(ct_eq<1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_shortest(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        MAKE_TUPLE(MAKE_TUPLE(ct_eq<0>{}), MAKE_TUPLE(ct_eq<1>{}), MAKE_TUPLE(ct_eq<2>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_shortest(MAKE_TUPLE(), MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_shortest(MAKE_TUPLE(ct_eq<0>{}), MAKE_TUPLE(ct_eq<1>{})),
        MAKE_TUPLE(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_shortest(MAKE_TUPLE(ct_eq<0>{}),
                           MAKE_TUPLE(ct_eq<1>{}),
                           MAKE_TUPLE(ct_eq<2>{})),
        MAKE_TUPLE(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::zip_shortest(MAKE_TUPLE(ct_eq<0>{}, ct_eq<3>{}),
                           MAKE_TUPLE(ct_eq<1>{}, ct_eq<4>{}),
                           MAKE_TUPLE(ct_eq<2>{}, ct_eq<5>{}, ct_eq<8>{})),
        MAKE_TUPLE(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
                   MAKE_TUPLE(ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}))
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_ZIPS_HPP
