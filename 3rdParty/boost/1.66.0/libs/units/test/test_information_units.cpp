// Boost.Units - A C++ library for zero-overhead dimensional analysis and 
// unit/quantity manipulation and conversion
//
// Copyright (C) 2014 Erik Erlandson
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <sstream>

#include <boost/units/quantity.hpp>
#include <boost/units/conversion.hpp>
#include <boost/units/io.hpp>

#include <boost/units/systems/si/prefixes.hpp>
#include <boost/units/systems/si/time.hpp>

// All information systems definitions
#include <boost/units/systems/information.hpp>

using std::cout;
using std::cerr;
using std::endl;
using std::stringstream;

namespace bu = boost::units;
namespace si = boost::units::si;

using bu::quantity;

using bu::information::bit_base_unit;
using bu::information::byte_base_unit;
using bu::information::nat_base_unit;
using bu::information::hartley_base_unit;
using bu::information::shannon_base_unit;


#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>


#include <boost/multiprecision/cpp_int.hpp>

const double close_fraction = 0.0000001;

// checks that cf(u2,u1) == expected
// also checks invariant property that cf(u2,u1) * cf(u1,u2) == 1 
#define CHECK_DIRECT_CF(u1, u2, expected) \
    BOOST_CHECK_CLOSE_FRACTION(bu::conversion_factor((u2), (u1)), (expected), close_fraction); \
    BOOST_CHECK_CLOSE_FRACTION(bu::conversion_factor((u2), (u1)) * bu::conversion_factor((u1), (u2)), 1.0, close_fraction);

// check transitive conversion factors
// invariant:  cf(u1,u3) = cf(u1,u2)*cf(u2,u3) 
#define CHECK_TRANSITIVE_CF(u1, u2, u3) { \
    double cf12 = bu::conversion_factor((u2), (u1)) ; \
    double cf23 = bu::conversion_factor((u3), (u2)) ; \
    double cf13 = bu::conversion_factor((u3), (u1)) ; \
    BOOST_CHECK_CLOSE_FRACTION(cf13, cf12*cf23, close_fraction); \
    double cf32 = bu::conversion_factor((u2), (u3)) ; \
    double cf21 = bu::conversion_factor((u1), (u2)) ; \
    double cf31 = bu::conversion_factor((u1), (u3)) ; \
    BOOST_CHECK_CLOSE_FRACTION(cf31, cf32*cf21, close_fraction); \
}


BOOST_AUTO_TEST_CASE(test_cf_bit_byte) {
    CHECK_DIRECT_CF(bit_base_unit::unit_type(), byte_base_unit::unit_type(), 8.0);
}

BOOST_AUTO_TEST_CASE(test_cf_bit_nat) {
    CHECK_DIRECT_CF(bit_base_unit::unit_type(), nat_base_unit::unit_type(), 1.442695040888964);
}

BOOST_AUTO_TEST_CASE(test_cf_bit_hartley) {
    CHECK_DIRECT_CF(bit_base_unit::unit_type(), hartley_base_unit::unit_type(), 3.321928094887363);
}

BOOST_AUTO_TEST_CASE(test_cf_bit_shannon) {
    CHECK_DIRECT_CF(bit_base_unit::unit_type(), shannon_base_unit::unit_type(), 1.0);
}

/////////////////////////////////////////////////////////////////////////////////////
// spot-check that these are automatically transitive, thru central "hub unit" bit:
// basic pattern is to test invariant property:  cf(c,a) = cf(c,b)*cf(b,a)

BOOST_AUTO_TEST_CASE(test_transitive_byte_nat) {
    CHECK_TRANSITIVE_CF(byte_base_unit::unit_type(), bit_base_unit::unit_type(), nat_base_unit::unit_type());
}
BOOST_AUTO_TEST_CASE(test_transitive_nat_hartley) {
    CHECK_TRANSITIVE_CF(nat_base_unit::unit_type(), bit_base_unit::unit_type(), hartley_base_unit::unit_type());
}
BOOST_AUTO_TEST_CASE(test_transitive_hartley_shannon) {
    CHECK_TRANSITIVE_CF(hartley_base_unit::unit_type(), bit_base_unit::unit_type(), shannon_base_unit::unit_type());
}
BOOST_AUTO_TEST_CASE(test_transitive_shannon_byte) {
    CHECK_TRANSITIVE_CF(shannon_base_unit::unit_type(), bit_base_unit::unit_type(), byte_base_unit::unit_type());
}

// test transitive factors, none of which are bit, just for good measure
BOOST_AUTO_TEST_CASE(test_transitive_byte_nat_hartley) {
    CHECK_TRANSITIVE_CF(byte_base_unit::unit_type(), nat_base_unit::unit_type(), hartley_base_unit::unit_type());
}

BOOST_AUTO_TEST_CASE(test_byte_quantity_is_default) {
    using namespace bu::information;
    quantity<info, double> qd(2 * byte);
    BOOST_CHECK_EQUAL(qd.value(), double(2));
    quantity<info, long> ql(2 * byte);
    BOOST_CHECK_EQUAL(ql.value(), long(2));
}

BOOST_AUTO_TEST_CASE(test_byte_quantity_explicit) {
    using namespace bu::information;
    quantity<hu::byte::info, double> qd(2 * byte);
    BOOST_CHECK_EQUAL(qd.value(), double(2));
    quantity<hu::byte::info, long> ql(2 * byte);
    BOOST_CHECK_EQUAL(ql.value(), long(2));
}

BOOST_AUTO_TEST_CASE(test_bit_quantity) {
    using namespace bu::information;
    quantity<hu::bit::info, double> qd(2 * bit);
    BOOST_CHECK_EQUAL(qd.value(), double(2));
    quantity<hu::bit::info, long> ql(2 * bit);
    BOOST_CHECK_EQUAL(ql.value(), long(2));
}

BOOST_AUTO_TEST_CASE(test_nat_quantity) {
    using namespace bu::information;
    quantity<hu::nat::info, double> qd(2 * nat);
    BOOST_CHECK_EQUAL(qd.value(), double(2));
    quantity<hu::nat::info, long> ql(2 * nat);
    BOOST_CHECK_EQUAL(ql.value(), long(2));
}

BOOST_AUTO_TEST_CASE(test_hartley_quantity) {
    using namespace bu::information;
    quantity<hu::hartley::info, double> qd(2 * hartley);
    BOOST_CHECK_EQUAL(qd.value(), double(2));
    quantity<hu::hartley::info, long> ql(2 * hartley);
    BOOST_CHECK_EQUAL(ql.value(), long(2));
}

BOOST_AUTO_TEST_CASE(test_shannon_quantity) {
    using namespace bu::information;
    quantity<hu::shannon::info, double> qd(2 * shannon);
    BOOST_CHECK_EQUAL(qd.value(), double(2));
    quantity<hu::shannon::info, long> ql(2 * shannon);
    BOOST_CHECK_EQUAL(ql.value(), long(2));
}

BOOST_AUTO_TEST_CASE(test_mixed_hu) {
    using namespace bu::information;
    const double cf = 0.001;
    BOOST_CHECK_CLOSE_FRACTION((quantity<hu::bit::info>(1.0 * bits)).value(), 1.0, cf);
    BOOST_CHECK_CLOSE_FRACTION((quantity<hu::byte::info>(1.0 * bits)).value(), 1.0/8.0, cf);
    BOOST_CHECK_CLOSE_FRACTION((quantity<hu::nat::info>(1.0 * bits)).value(), 0.69315, cf);
    BOOST_CHECK_CLOSE_FRACTION((quantity<hu::hartley::info>(1.0 * bits)).value(), 0.30102, cf);
    BOOST_CHECK_CLOSE_FRACTION((quantity<hu::shannon::info>(1.0 * bits)).value(), 1.0, cf);
}

BOOST_AUTO_TEST_CASE(test_info_prefixes) {
    using namespace bu::information;
    quantity<info, long long> q10(1LL * kibi * byte);
    BOOST_CHECK_EQUAL(q10.value(), 1024LL);

    quantity<info, long long> q20(1LL * mebi * byte);
    BOOST_CHECK_EQUAL(q20.value(), 1048576LL);

    quantity<info, long long> q30(1LL * gibi * byte);
    BOOST_CHECK_EQUAL(q30.value(), 1073741824LL);

    quantity<info, long long> q40(1LL * tebi * byte);
    BOOST_CHECK_EQUAL(q40.value(), 1099511627776LL);

    quantity<info, long long> q50(1LL * pebi * byte);
    BOOST_CHECK_EQUAL(q50.value(), 1125899906842624LL);

    quantity<info, long long> q60(1LL * exbi * byte);
    BOOST_CHECK_EQUAL(q60.value(), 1152921504606846976LL);

    using boost::multiprecision::int128_t;

    quantity<info, int128_t> q70(1LL * zebi * byte);
    BOOST_CHECK_EQUAL(q70.value(), int128_t("1180591620717411303424"));

    quantity<info, int128_t> q80(1LL * yobi * byte);
    BOOST_CHECK_EQUAL(q80.value(), int128_t("1208925819614629174706176"));

    // sanity check: si prefixes should also operate
    quantity<info, long long> q1e3(1LL * si::kilo * byte);
    BOOST_CHECK_EQUAL(q1e3.value(), 1000LL);

    quantity<info, long long> q1e6(1LL * si::mega * byte);
    BOOST_CHECK_EQUAL(q1e6.value(), 1000000LL);
}

BOOST_AUTO_TEST_CASE(test_unit_constant_io) {
    using namespace bu::information;

    std::stringstream ss;
    ss << bu::symbol_format << bytes;
    BOOST_CHECK_EQUAL(ss.str(), "B");

    ss.str("");
    ss << bu::name_format << bytes;
    BOOST_CHECK_EQUAL(ss.str(), "byte");

    ss.str("");
    ss << bu::symbol_format << bits;
    BOOST_CHECK_EQUAL(ss.str(), "b");

    ss.str("");
    ss << bu::name_format << bits;
    BOOST_CHECK_EQUAL(ss.str(), "bit");

    ss.str("");
    ss << bu::symbol_format << nats;
    BOOST_CHECK_EQUAL(ss.str(), "nat");

    ss.str("");
    ss << bu::name_format << nats;
    BOOST_CHECK_EQUAL(ss.str(), "nat");

    ss.str("");
    ss << bu::symbol_format << hartleys;
    BOOST_CHECK_EQUAL(ss.str(), "Hart");

    ss.str("");
    ss << bu::name_format << hartleys;
    BOOST_CHECK_EQUAL(ss.str(), "hartley");

    ss.str("");
    ss << bu::symbol_format << shannons;
    BOOST_CHECK_EQUAL(ss.str(), "Sh");

    ss.str("");
    ss << bu::name_format << shannons;
    BOOST_CHECK_EQUAL(ss.str(), "shannon");
}
