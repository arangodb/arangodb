// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/negate.hpp>

#include <type_traits>
namespace hana = boost::hana;
using namespace hana::literals;


BOOST_HANA_CONSTANT_CHECK(0_c == hana::llong_c<0>);
BOOST_HANA_CONSTANT_CHECK(1_c == hana::llong_c<1>);
BOOST_HANA_CONSTANT_CHECK(12_c == hana::llong_c<12>);
BOOST_HANA_CONSTANT_CHECK(123_c == hana::llong_c<123>);
BOOST_HANA_CONSTANT_CHECK(1234567_c == hana::llong_c<1234567>);
BOOST_HANA_CONSTANT_CHECK(-34_c == hana::llong_c<-34>);


static_assert(std::is_same<
    decltype(-1234_c)::value_type,
    long long
>{}, "");
static_assert(-1234_c == -1234ll, "");
BOOST_HANA_CONSTANT_CHECK(-12_c < 0_c);


constexpr auto deadbeef = hana::llong_c<0xDEADBEEF>;

//hexadecimal
BOOST_HANA_CONSTANT_CHECK(deadbeef == 0xDEADBEEF_c);
BOOST_HANA_CONSTANT_CHECK(deadbeef == 0xDeAdBeEf_c);
BOOST_HANA_CONSTANT_CHECK(deadbeef == 0xdeadbeef_c);

//decimal
BOOST_HANA_CONSTANT_CHECK(deadbeef == hana::llong_c<3735928559>); //"test the test"
BOOST_HANA_CONSTANT_CHECK(deadbeef == 3735928559_c);

//binary
BOOST_HANA_CONSTANT_CHECK(deadbeef == hana::llong_c<0b11011110101011011011111011101111>); //"test the test"
BOOST_HANA_CONSTANT_CHECK(deadbeef == 0b11011110101011011011111011101111_c);

//octal
BOOST_HANA_CONSTANT_CHECK(deadbeef == hana::llong_c<033653337357>); //"test the test"
BOOST_HANA_CONSTANT_CHECK(deadbeef == 033653337357_c);

BOOST_HANA_CONSTANT_CHECK(0x0_c == hana::llong_c<0>);
BOOST_HANA_CONSTANT_CHECK(0b0_c == hana::llong_c<0>);
BOOST_HANA_CONSTANT_CHECK(00_c == hana::llong_c<0>);

BOOST_HANA_CONSTANT_CHECK(0x1_c == hana::llong_c<1>);
BOOST_HANA_CONSTANT_CHECK(0b1_c == hana::llong_c<1>);
BOOST_HANA_CONSTANT_CHECK(01_c == hana::llong_c<1>);

BOOST_HANA_CONSTANT_CHECK(-0x1_c == hana::llong_c<-1>);
BOOST_HANA_CONSTANT_CHECK(-0b1_c == hana::llong_c<-1>);
BOOST_HANA_CONSTANT_CHECK(-01_c == hana::llong_c<-1>);

BOOST_HANA_CONSTANT_CHECK(0x10_c == hana::llong_c<16>);
BOOST_HANA_CONSTANT_CHECK(0b10_c == hana::llong_c<2>);
BOOST_HANA_CONSTANT_CHECK(010_c == hana::llong_c<8>);

BOOST_HANA_CONSTANT_CHECK(-0x10_c == hana::llong_c<-16>);
BOOST_HANA_CONSTANT_CHECK(-0b10_c == hana::llong_c<-2>);
BOOST_HANA_CONSTANT_CHECK(-010_c == hana::llong_c<-8>);

int main() { }
