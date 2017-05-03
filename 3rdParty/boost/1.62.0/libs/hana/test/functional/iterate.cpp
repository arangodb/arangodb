// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/functional/iterate.hpp>
#include <boost/hana/assert.hpp>

#include <laws/base.hpp>

#include <vector>
using namespace boost::hana;


struct undefined { };

constexpr int incr(int i) { return i + 1; }

int main() {
    test::_injection<0> f{};
    using test::ct_eq;

    // "real usage" tests
    static_assert(iterate<3>(incr, 0) == 3, "");
    {
        std::vector<int> vec;
        iterate<10>([&](int i) { vec.push_back(i); return i + 1; }, 0);
        BOOST_HANA_RUNTIME_CHECK(
            vec == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
        );
    }

    // equivalence between iterate<n>(f, x) and iterate<n>(f)(x)
    BOOST_HANA_CONSTANT_CHECK(equal(
        iterate<0>(undefined{})(ct_eq<0>{}),
        iterate<0>(undefined{}, ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(equal(
        iterate<4>(f)(ct_eq<0>{}),
        iterate<4>(f, ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(equal(
        iterate<10>(f)(ct_eq<0>{}),
        iterate<10>(f, ct_eq<0>{})
    ));

    // systematic tests
    BOOST_HANA_CONSTANT_CHECK(equal(
        iterate<0>(undefined{}, ct_eq<0>{}),
        ct_eq<0>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(equal(
        iterate<1>(f, ct_eq<0>{}),
        f(ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(equal(
        iterate<2>(f, ct_eq<0>{}),
        f(f(ct_eq<0>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(equal(
        iterate<3>(f, ct_eq<0>{}),
        f(f(f(ct_eq<0>{})))
    ));

    BOOST_HANA_CONSTANT_CHECK(equal(
        iterate<4>(f, ct_eq<0>{}),
        f(f(f(f(ct_eq<0>{}))))
    ));

    BOOST_HANA_CONSTANT_CHECK(equal(
        iterate<5>(f, ct_eq<0>{}),
        f(f(f(f(f(ct_eq<0>{})))))
    ));

    BOOST_HANA_CONSTANT_CHECK(equal(
        iterate<6>(f, ct_eq<0>{}),
        f(f(f(f(f(f(ct_eq<0>{}))))))
    ));

    BOOST_HANA_CONSTANT_CHECK(equal(
        iterate<7>(f, ct_eq<0>{}),
        f(f(f(f(f(f(f(ct_eq<0>{})))))))
    ));

    BOOST_HANA_CONSTANT_CHECK(equal(
        iterate<11>(f, ct_eq<0>{}),
        f(f(f(f(f(f(f(f(f(f(f(ct_eq<0>{})))))))))))
    ));

    BOOST_HANA_CONSTANT_CHECK(equal(
        iterate<12>(f, ct_eq<0>{}),
        f(f(f(f(f(f(f(f(f(f(f(f(ct_eq<0>{}))))))))))))
    ));

    BOOST_HANA_CONSTANT_CHECK(equal(
        iterate<13>(f, ct_eq<0>{}),
        f(f(f(f(f(f(f(f(f(f(f(f(f(ct_eq<0>{})))))))))))))
    ));

    // We can't nest too many calls to f, because that uses a hana::tuple
    // internally and some implementation (libstdc++) have trouble with
    // deeply-nested calls to `std::is_constructible`, which is required by
    // hana::tuple. Hence, we use an homogeneous function for the remaining
    // tests.
    static_assert(iterate<23>(incr, 0) == 23, "");
    static_assert(iterate<24>(incr, 0) == 24, "");
    static_assert(iterate<25>(incr, 0) == 25, "");
    static_assert(iterate<26>(incr, 0) == 26, "");
    static_assert(iterate<27>(incr, 0) == 27, "");
    static_assert(iterate<28>(incr, 0) == 28, "");
    static_assert(iterate<29>(incr, 0) == 29, "");

    static_assert(iterate<30>(incr, 0) == 30, "");
    static_assert(iterate<31>(incr, 0) == 31, "");
    static_assert(iterate<32>(incr, 0) == 32, "");
    static_assert(iterate<33>(incr, 0) == 33, "");
    static_assert(iterate<34>(incr, 0) == 34, "");
    static_assert(iterate<35>(incr, 0) == 35, "");
    static_assert(iterate<36>(incr, 0) == 36, "");
    static_assert(iterate<37>(incr, 0) == 37, "");
    static_assert(iterate<38>(incr, 0) == 38, "");
    static_assert(iterate<39>(incr, 0) == 39, "");

    static_assert(iterate<40>(incr, 0) == 40, "");
    static_assert(iterate<41>(incr, 0) == 41, "");
    static_assert(iterate<42>(incr, 0) == 42, "");
    static_assert(iterate<43>(incr, 0) == 43, "");
    static_assert(iterate<44>(incr, 0) == 44, "");
    static_assert(iterate<45>(incr, 0) == 45, "");
    static_assert(iterate<46>(incr, 0) == 46, "");
    static_assert(iterate<47>(incr, 0) == 47, "");
    static_assert(iterate<48>(incr, 0) == 48, "");
    static_assert(iterate<49>(incr, 0) == 49, "");

    static_assert(iterate<50>(incr, 0) == 50, "");
    static_assert(iterate<51>(incr, 0) == 51, "");
    static_assert(iterate<52>(incr, 0) == 52, "");
    static_assert(iterate<53>(incr, 0) == 53, "");
    static_assert(iterate<54>(incr, 0) == 54, "");
    static_assert(iterate<55>(incr, 0) == 55, "");
    static_assert(iterate<56>(incr, 0) == 56, "");
    static_assert(iterate<57>(incr, 0) == 57, "");
    static_assert(iterate<58>(incr, 0) == 58, "");
    static_assert(iterate<59>(incr, 0) == 59, "");
}
