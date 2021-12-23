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


#include "test_close.hpp"

#include <boost/multiprecision/cpp_int.hpp>

const double close_fraction = 0.0000001;

// checks that cf(u2,u1) == expected
// also checks invariant property that cf(u2,u1) * cf(u1,u2) == 1 
#define CHECK_DIRECT_CF(u1, u2, expected) \
    BOOST_UNITS_TEST_CLOSE(bu::conversion_factor((u2), (u1)), (expected), close_fraction); \
    BOOST_UNITS_TEST_CLOSE(bu::conversion_factor((u2), (u1)) * bu::conversion_factor((u1), (u2)), 1.0, close_fraction);

// check transitive conversion factors
// invariant:  cf(u1,u3) = cf(u1,u2)*cf(u2,u3) 
#define CHECK_TRANSITIVE_CF(u1, u2, u3) { \
    BOOST_CONSTEXPR_OR_CONST double cf12 = bu::conversion_factor((u2), (u1)) ; \
    BOOST_CONSTEXPR_OR_CONST double cf23 = bu::conversion_factor((u3), (u2)) ; \
    BOOST_CONSTEXPR_OR_CONST double cf13 = bu::conversion_factor((u3), (u1)) ; \
    BOOST_UNITS_TEST_CLOSE(cf13, cf12*cf23, close_fraction); \
    BOOST_CONSTEXPR_OR_CONST double cf32 = bu::conversion_factor((u2), (u3)) ; \
    BOOST_CONSTEXPR_OR_CONST double cf21 = bu::conversion_factor((u1), (u2)) ; \
    BOOST_CONSTEXPR_OR_CONST double cf31 = bu::conversion_factor((u1), (u3)) ; \
    BOOST_UNITS_TEST_CLOSE(cf31, cf32*cf21, close_fraction); \
}


void test_cf_bit_byte() {
    CHECK_DIRECT_CF(bit_base_unit::unit_type(), byte_base_unit::unit_type(), 8.0);
}

void test_cf_bit_nat() {
    CHECK_DIRECT_CF(bit_base_unit::unit_type(), nat_base_unit::unit_type(), 1.442695040888964);
}

void test_cf_bit_hartley() {
    CHECK_DIRECT_CF(bit_base_unit::unit_type(), hartley_base_unit::unit_type(), 3.321928094887363);
}

void test_cf_bit_shannon() {
    CHECK_DIRECT_CF(bit_base_unit::unit_type(), shannon_base_unit::unit_type(), 1.0);
}

/////////////////////////////////////////////////////////////////////////////////////
// spot-check that these are automatically transitive, thru central "hub unit" bit:
// basic pattern is to test invariant property:  cf(c,a) = cf(c,b)*cf(b,a)

void test_transitive_byte_nat() {
    CHECK_TRANSITIVE_CF(byte_base_unit::unit_type(), bit_base_unit::unit_type(), nat_base_unit::unit_type());
}
void test_transitive_nat_hartley() {
    CHECK_TRANSITIVE_CF(nat_base_unit::unit_type(), bit_base_unit::unit_type(), hartley_base_unit::unit_type());
}
void test_transitive_hartley_shannon() {
    CHECK_TRANSITIVE_CF(hartley_base_unit::unit_type(), bit_base_unit::unit_type(), shannon_base_unit::unit_type());
}
void test_transitive_shannon_byte() {
    CHECK_TRANSITIVE_CF(shannon_base_unit::unit_type(), bit_base_unit::unit_type(), byte_base_unit::unit_type());
}

// test transitive factors, none of which are bit, just for good measure
void test_transitive_byte_nat_hartley() {
    CHECK_TRANSITIVE_CF(byte_base_unit::unit_type(), nat_base_unit::unit_type(), hartley_base_unit::unit_type());
}

void test_byte_quantity_is_default() {
    using namespace bu::information;
    using bu::information::byte;
    BOOST_CONSTEXPR_OR_CONST quantity<info, double> qd(2 * byte);
    BOOST_TEST_EQ(qd.value(), double(2));
    BOOST_CONSTEXPR_OR_CONST quantity<info, long> ql(2 * byte);
    BOOST_TEST_EQ(ql.value(), long(2));
}

void test_byte_quantity_explicit() {
    using namespace bu::information;
    using bu::information::byte;
    BOOST_CONSTEXPR_OR_CONST quantity<hu::byte::info, double> qd(2 * byte);
    BOOST_TEST_EQ(qd.value(), double(2));
    BOOST_CONSTEXPR_OR_CONST quantity<hu::byte::info, long> ql(2 * byte);
    BOOST_TEST_EQ(ql.value(), long(2));
}

void test_bit_quantity() {
    using namespace bu::information;
    BOOST_CONSTEXPR_OR_CONST quantity<hu::bit::info, double> qd(2 * bit);
    BOOST_TEST_EQ(qd.value(), double(2));
    BOOST_CONSTEXPR_OR_CONST quantity<hu::bit::info, long> ql(2 * bit);
    BOOST_TEST_EQ(ql.value(), long(2));
}

void test_nat_quantity() {
    using namespace bu::information;
    BOOST_CONSTEXPR_OR_CONST quantity<hu::nat::info, double> qd(2 * nat);
    BOOST_TEST_EQ(qd.value(), double(2));
    BOOST_CONSTEXPR_OR_CONST quantity<hu::nat::info, long> ql(2 * nat);
    BOOST_TEST_EQ(ql.value(), long(2));
}

void test_hartley_quantity() {
    using namespace bu::information;
    BOOST_CONSTEXPR_OR_CONST quantity<hu::hartley::info, double> qd(2 * hartley);
    BOOST_TEST_EQ(qd.value(), double(2));
    BOOST_CONSTEXPR_OR_CONST quantity<hu::hartley::info, long> ql(2 * hartley);
    BOOST_TEST_EQ(ql.value(), long(2));
}

void test_shannon_quantity() {
    using namespace bu::information;
    BOOST_CONSTEXPR_OR_CONST quantity<hu::shannon::info, double> qd(2 * shannon);
    BOOST_TEST_EQ(qd.value(), double(2));
    BOOST_CONSTEXPR_OR_CONST quantity<hu::shannon::info, long> ql(2 * shannon);
    BOOST_TEST_EQ(ql.value(), long(2));
}

void test_mixed_hu() {
    using namespace bu::information;
    BOOST_CONSTEXPR_OR_CONST double cf = 0.001;
    BOOST_UNITS_TEST_CLOSE((quantity<hu::bit::info>(1.0 * bits)).value(), 1.0, cf);
    BOOST_UNITS_TEST_CLOSE((quantity<hu::byte::info>(1.0 * bits)).value(), 1.0/8.0, cf);
    BOOST_UNITS_TEST_CLOSE((quantity<hu::nat::info>(1.0 * bits)).value(), 0.69315, cf);
    BOOST_UNITS_TEST_CLOSE((quantity<hu::hartley::info>(1.0 * bits)).value(), 0.30102, cf);
    BOOST_UNITS_TEST_CLOSE((quantity<hu::shannon::info>(1.0 * bits)).value(), 1.0, cf);
}

void test_info_prefixes() {
    using namespace bu::information;
    using bu::information::byte;
    BOOST_CONSTEXPR_OR_CONST quantity<info, long long> q10(1LL * kibi * byte);
    BOOST_TEST_EQ(q10.value(), 1024LL);

    BOOST_CONSTEXPR_OR_CONST quantity<info, long long> q20(1LL * mebi * byte);
    BOOST_TEST_EQ(q20.value(), 1048576LL);

    BOOST_CONSTEXPR_OR_CONST quantity<info, long long> q30(1LL * gibi * byte);
    BOOST_TEST_EQ(q30.value(), 1073741824LL);

    BOOST_CONSTEXPR_OR_CONST quantity<info, long long> q40(1LL * tebi * byte);
    BOOST_TEST_EQ(q40.value(), 1099511627776LL);

    BOOST_CONSTEXPR_OR_CONST quantity<info, long long> q50(1LL * pebi * byte);
    BOOST_TEST_EQ(q50.value(), 1125899906842624LL);

    BOOST_CONSTEXPR_OR_CONST quantity<info, long long> q60(1LL * exbi * byte);
    BOOST_TEST_EQ(q60.value(), 1152921504606846976LL);

    using boost::multiprecision::int128_t;

    const quantity<info, int128_t> q70(1LL * zebi * byte);
    BOOST_TEST_EQ(q70.value(), int128_t("1180591620717411303424"));

    const quantity<info, int128_t> q80(1LL * yobi * byte);
    BOOST_TEST_EQ(q80.value(), int128_t("1208925819614629174706176"));

    // sanity check: si prefixes should also operate
    BOOST_CONSTEXPR_OR_CONST quantity<info, long long> q1e3(1LL * si::kilo * byte);
    BOOST_TEST_EQ(q1e3.value(), 1000LL);

    BOOST_CONSTEXPR_OR_CONST quantity<info, long long> q1e6(1LL * si::mega * byte);
    BOOST_TEST_EQ(q1e6.value(), 1000000LL);
}

void test_unit_constant_io() {
    using namespace bu::information;

    std::stringstream ss;
    ss << bu::symbol_format << bytes;
    BOOST_TEST_EQ(ss.str(), "B");

    ss.str("");
    ss << bu::name_format << bytes;
    BOOST_TEST_EQ(ss.str(), "byte");

    ss.str("");
    ss << bu::symbol_format << bits;
    BOOST_TEST_EQ(ss.str(), "b");

    ss.str("");
    ss << bu::name_format << bits;
    BOOST_TEST_EQ(ss.str(), "bit");

    ss.str("");
    ss << bu::symbol_format << nats;
    BOOST_TEST_EQ(ss.str(), "nat");

    ss.str("");
    ss << bu::name_format << nats;
    BOOST_TEST_EQ(ss.str(), "nat");

    ss.str("");
    ss << bu::symbol_format << hartleys;
    BOOST_TEST_EQ(ss.str(), "Hart");

    ss.str("");
    ss << bu::name_format << hartleys;
    BOOST_TEST_EQ(ss.str(), "hartley");

    ss.str("");
    ss << bu::symbol_format << shannons;
    BOOST_TEST_EQ(ss.str(), "Sh");

    ss.str("");
    ss << bu::name_format << shannons;
    BOOST_TEST_EQ(ss.str(), "shannon");
}

int main()
{
    test_cf_bit_byte();
    test_cf_bit_nat();
    test_cf_bit_hartley();
    test_cf_bit_shannon();
    test_transitive_byte_nat();
    test_transitive_nat_hartley();
    test_transitive_hartley_shannon();
    test_transitive_shannon_byte();
    test_transitive_byte_nat_hartley();
    test_byte_quantity_is_default();
    test_byte_quantity_explicit();
    test_bit_quantity();
    test_nat_quantity();
    test_hartley_quantity();
    test_shannon_quantity();
    test_mixed_hu();
    test_info_prefixes();
    test_unit_constant_io();
    return boost::report_errors();
}
