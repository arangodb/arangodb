// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_SCANS_HPP
#define BOOST_HANA_TEST_AUTO_SCANS_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/scan_left.hpp>
#include <boost/hana/scan_right.hpp>

#include <laws/base.hpp>
#include "test_case.hpp"


TestCase test_scan_left{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    struct undefined { };
    hana::test::_injection<0> f{};

    // Without initial state
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_left(MAKE_TUPLE(), undefined{}),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_left(MAKE_TUPLE(ct_eq<0>{}), undefined{}),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_left(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), f),
        MAKE_TUPLE(ct_eq<0>{}, f(ct_eq<0>{}, ct_eq<1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_left(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), f),
        MAKE_TUPLE(
            ct_eq<0>{},
            f(ct_eq<0>{}, ct_eq<1>{}),
            f(f(ct_eq<0>{}, ct_eq<1>{}), ct_eq<2>{})
        )
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_left(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), f),
        MAKE_TUPLE(
            ct_eq<0>{},
            f(ct_eq<0>{}, ct_eq<1>{}),
            f(f(ct_eq<0>{}, ct_eq<1>{}), ct_eq<2>{}),
            f(f(f(ct_eq<0>{}, ct_eq<1>{}), ct_eq<2>{}), ct_eq<3>{})
        )
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_left(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}), f),
        MAKE_TUPLE(
            ct_eq<0>{},
            f(ct_eq<0>{}, ct_eq<1>{}),
            f(f(ct_eq<0>{}, ct_eq<1>{}), ct_eq<2>{}),
            f(f(f(ct_eq<0>{}, ct_eq<1>{}), ct_eq<2>{}), ct_eq<3>{}),
            f(f(f(f(ct_eq<0>{}, ct_eq<1>{}), ct_eq<2>{}), ct_eq<3>{}), ct_eq<4>{})
        )
    ));

    // With initial state
    auto s = ct_eq<999>{};
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_left(MAKE_TUPLE(), s, undefined{}),
        MAKE_TUPLE(s)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_left(MAKE_TUPLE(ct_eq<0>{}), s, f),
        MAKE_TUPLE(s, f(s, ct_eq<0>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_left(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), s, f),
        MAKE_TUPLE(s, f(s, ct_eq<0>{}), f(f(s, ct_eq<0>{}), ct_eq<1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_left(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), s, f),
        MAKE_TUPLE(
            s,
            f(s, ct_eq<0>{}),
            f(f(s, ct_eq<0>{}), ct_eq<1>{}),
            f(f(f(s, ct_eq<0>{}), ct_eq<1>{}), ct_eq<2>{})
        )
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_left(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), s, f),
        MAKE_TUPLE(
            s,
            f(s, ct_eq<0>{}),
            f(f(s, ct_eq<0>{}), ct_eq<1>{}),
            f(f(f(s, ct_eq<0>{}), ct_eq<1>{}), ct_eq<2>{}),
            f(f(f(f(s, ct_eq<0>{}), ct_eq<1>{}), ct_eq<2>{}), ct_eq<3>{})
        )
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_left(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}), s, f),
        MAKE_TUPLE(
            s,
            f(s, ct_eq<0>{}),
            f(f(s, ct_eq<0>{}), ct_eq<1>{}),
            f(f(f(s, ct_eq<0>{}), ct_eq<1>{}), ct_eq<2>{}),
            f(f(f(f(s, ct_eq<0>{}), ct_eq<1>{}), ct_eq<2>{}), ct_eq<3>{}),
            f(f(f(f(f(s, ct_eq<0>{}), ct_eq<1>{}), ct_eq<2>{}), ct_eq<3>{}), ct_eq<4>{})
        )
    ));
}};


TestCase test_scan_right{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    struct undefined { };
    hana::test::_injection<0> f{};

    // Without initial state
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_right(MAKE_TUPLE(), undefined{}),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_right(MAKE_TUPLE(ct_eq<0>{}), undefined{}),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_right(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), f),
        MAKE_TUPLE(
            f(ct_eq<0>{}, ct_eq<1>{}),
                          ct_eq<1>{}
        )
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_right(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), f),
        MAKE_TUPLE(
            f(ct_eq<0>{}, f(ct_eq<1>{}, ct_eq<2>{})),
                          f(ct_eq<1>{}, ct_eq<2>{}),
                                        ct_eq<2>{}
        )
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_right(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), f),
        MAKE_TUPLE(
            f(ct_eq<0>{}, f(ct_eq<1>{}, f(ct_eq<2>{}, ct_eq<3>{}))),
                          f(ct_eq<1>{}, f(ct_eq<2>{}, ct_eq<3>{})),
                                        f(ct_eq<2>{}, ct_eq<3>{}),
                                                      ct_eq<3>{}
        )
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_right(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}), f),
        MAKE_TUPLE(
            f(ct_eq<0>{}, f(ct_eq<1>{}, f(ct_eq<2>{}, f(ct_eq<3>{}, ct_eq<4>{})))),
                          f(ct_eq<1>{}, f(ct_eq<2>{}, f(ct_eq<3>{}, ct_eq<4>{}))),
                                        f(ct_eq<2>{}, f(ct_eq<3>{}, ct_eq<4>{})),
                                                      f(ct_eq<3>{}, ct_eq<4>{}),
                                                                    ct_eq<4>{}
        )
    ));

    // With initial state
    auto s = ct_eq<999>{};
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_right(MAKE_TUPLE(), s, undefined{}),
        MAKE_TUPLE(s)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_right(MAKE_TUPLE(ct_eq<0>{}), s, f),
        MAKE_TUPLE(
            f(ct_eq<0>{}, s),
            s
        )
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_right(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), s, f),
        MAKE_TUPLE(
            f(ct_eq<0>{}, f(ct_eq<1>{}, s)),
                          f(ct_eq<1>{}, s),
                                        s
        )
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_right(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), s, f),
        MAKE_TUPLE(
            f(ct_eq<0>{}, f(ct_eq<1>{}, f(ct_eq<2>{}, s))),
                          f(ct_eq<1>{}, f(ct_eq<2>{}, s)),
                                        f(ct_eq<2>{}, s),
                                                      s
        )
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_right(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), s, f),
        MAKE_TUPLE(
            f(ct_eq<0>{}, f(ct_eq<1>{}, f(ct_eq<2>{}, f(ct_eq<3>{}, s)))),
                          f(ct_eq<1>{}, f(ct_eq<2>{}, f(ct_eq<3>{}, s))),
                                        f(ct_eq<2>{}, f(ct_eq<3>{}, s)),
                                                      f(ct_eq<3>{}, s),
                                                                    s
        )
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::scan_right(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}), s, f),
        MAKE_TUPLE(
            f(ct_eq<0>{}, f(ct_eq<1>{}, f(ct_eq<2>{}, f(ct_eq<3>{}, f(ct_eq<4>{}, s))))),
                          f(ct_eq<1>{}, f(ct_eq<2>{}, f(ct_eq<3>{}, f(ct_eq<4>{}, s)))),
                                        f(ct_eq<2>{}, f(ct_eq<3>{}, f(ct_eq<4>{}, s))),
                                                      f(ct_eq<3>{}, f(ct_eq<4>{}, s)),
                                                                    f(ct_eq<4>{}, s),
                                                                                  s
        )
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_SCANS_HPP
