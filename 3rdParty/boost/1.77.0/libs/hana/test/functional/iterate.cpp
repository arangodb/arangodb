// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/functional/iterate.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>

#include <laws/base.hpp>

#include <vector>
namespace hana = boost::hana;
using hana::test::ct_eq;


struct undefined { };

constexpr int incr(int i) { return i + 1; }

int main() {
    hana::test::_injection<0> f{};

    // "real usage" tests
    static_assert(hana::iterate<3>(incr, 0) == 3, "");
    {
        std::vector<int> vec;
        hana::iterate<10>([&](int i) { vec.push_back(i); return i + 1; }, 0);
        BOOST_HANA_RUNTIME_CHECK(
            vec == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
        );
    }

    // equivalence between iterate<n>(f, x) and iterate<n>(f)(x)
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::iterate<0>(undefined{})(ct_eq<0>{}),
        hana::iterate<0>(undefined{}, ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::iterate<4>(f)(ct_eq<0>{}),
        hana::iterate<4>(f, ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::iterate<10>(f)(ct_eq<0>{}),
        hana::iterate<10>(f, ct_eq<0>{})
    ));

    // systematic tests
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::iterate<0>(undefined{}, ct_eq<0>{}),
        ct_eq<0>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::iterate<1>(f, ct_eq<0>{}),
        f(ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::iterate<2>(f, ct_eq<0>{}),
        f(f(ct_eq<0>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::iterate<3>(f, ct_eq<0>{}),
        f(f(f(ct_eq<0>{})))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::iterate<4>(f, ct_eq<0>{}),
        f(f(f(f(ct_eq<0>{}))))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::iterate<5>(f, ct_eq<0>{}),
        f(f(f(f(f(ct_eq<0>{})))))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::iterate<6>(f, ct_eq<0>{}),
        f(f(f(f(f(f(ct_eq<0>{}))))))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::iterate<7>(f, ct_eq<0>{}),
        f(f(f(f(f(f(f(ct_eq<0>{})))))))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::iterate<11>(f, ct_eq<0>{}),
        f(f(f(f(f(f(f(f(f(f(f(ct_eq<0>{})))))))))))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::iterate<12>(f, ct_eq<0>{}),
        f(f(f(f(f(f(f(f(f(f(f(f(ct_eq<0>{}))))))))))))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::iterate<13>(f, ct_eq<0>{}),
        f(f(f(f(f(f(f(f(f(f(f(f(f(ct_eq<0>{})))))))))))))
    ));

    // We can't nest too many calls to f, because that uses a hana::tuple
    // internally and some implementation (libstdc++) have trouble with
    // deeply-nested calls to `std::is_constructible`, which is required by
    // hana::tuple. Hence, we use an homogeneous function for the remaining
    // tests.
    static_assert(hana::iterate<23>(incr, 0) == 23, "");
    static_assert(hana::iterate<24>(incr, 0) == 24, "");
    static_assert(hana::iterate<25>(incr, 0) == 25, "");
    static_assert(hana::iterate<26>(incr, 0) == 26, "");
    static_assert(hana::iterate<27>(incr, 0) == 27, "");
    static_assert(hana::iterate<28>(incr, 0) == 28, "");
    static_assert(hana::iterate<29>(incr, 0) == 29, "");

    static_assert(hana::iterate<30>(incr, 0) == 30, "");
    static_assert(hana::iterate<31>(incr, 0) == 31, "");
    static_assert(hana::iterate<32>(incr, 0) == 32, "");
    static_assert(hana::iterate<33>(incr, 0) == 33, "");
    static_assert(hana::iterate<34>(incr, 0) == 34, "");
    static_assert(hana::iterate<35>(incr, 0) == 35, "");
    static_assert(hana::iterate<36>(incr, 0) == 36, "");
    static_assert(hana::iterate<37>(incr, 0) == 37, "");
    static_assert(hana::iterate<38>(incr, 0) == 38, "");
    static_assert(hana::iterate<39>(incr, 0) == 39, "");

    static_assert(hana::iterate<40>(incr, 0) == 40, "");
    static_assert(hana::iterate<41>(incr, 0) == 41, "");
    static_assert(hana::iterate<42>(incr, 0) == 42, "");
    static_assert(hana::iterate<43>(incr, 0) == 43, "");
    static_assert(hana::iterate<44>(incr, 0) == 44, "");
    static_assert(hana::iterate<45>(incr, 0) == 45, "");
    static_assert(hana::iterate<46>(incr, 0) == 46, "");
    static_assert(hana::iterate<47>(incr, 0) == 47, "");
    static_assert(hana::iterate<48>(incr, 0) == 48, "");
    static_assert(hana::iterate<49>(incr, 0) == 49, "");

    static_assert(hana::iterate<50>(incr, 0) == 50, "");
    static_assert(hana::iterate<51>(incr, 0) == 51, "");
    static_assert(hana::iterate<52>(incr, 0) == 52, "");
    static_assert(hana::iterate<53>(incr, 0) == 53, "");
    static_assert(hana::iterate<54>(incr, 0) == 54, "");
    static_assert(hana::iterate<55>(incr, 0) == 55, "");
    static_assert(hana::iterate<56>(incr, 0) == 56, "");
    static_assert(hana::iterate<57>(incr, 0) == 57, "");
    static_assert(hana::iterate<58>(incr, 0) == 58, "");
    static_assert(hana::iterate<59>(incr, 0) == 59, "");
}
