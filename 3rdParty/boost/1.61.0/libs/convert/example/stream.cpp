// Copyright (c) 2009-2014 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#include <boost/convert.hpp>
#include <boost/convert/lexical_cast.hpp>

//[stream_headers1
#include <boost/convert.hpp>
#include <boost/convert/stream.hpp>
#include <boost/detail/lightweight_test.hpp>

using std::string;
using boost::convert;

struct boost::cnv::by_default : public boost::cnv::cstream {};
//]
//[stream_headers2
namespace cnv = boost::cnv;
namespace arg = boost::cnv::parameter;
//]

#include "../test/test.hpp"

static
void
example1()
{
    //[stream_example1
    int    i2 = convert<int>("123").value();      // Throws when fails.
    int    i3 = convert<int>("uhm").value_or(-1); // Returns -1 when fails.
    string s2 = convert<string>(123).value();

    BOOST_TEST(i2 == 123);
    BOOST_TEST(i3 == -1);
    BOOST_TEST(s2 == "123");
    //]
}

static
void
example2()
{
    //[stream_example2
    boost::cnv::cstream ccnv;
    boost::cnv::wstream wcnv;

    int v01 = convert<int>("  FF", ccnv(std::hex)(std::skipws)).value_or(0);
    int v02 = convert<int>(L"  F", wcnv(std::hex)(std::skipws)).value_or(0);
    int v03 = convert<int>("  FF", ccnv(std::dec)(std::skipws)).value_or(-5);
    int v04 = convert<int>(L"  F", wcnv(std::dec)(std::skipws)).value_or(-5);

    BOOST_TEST(v01 == 255); // "FF"
    BOOST_TEST(v02 ==  15); // L"F"
    BOOST_TEST(v03 ==  -5); // Failed to convert "FF" as decimal.
    BOOST_TEST(v04 ==  -5); // Failed to convert L"F" as decimal.
    //]
    //[stream_example3
    ccnv(std::showbase)(std::uppercase)(std::hex);

    BOOST_TEST(convert<string>(255, ccnv, "bad") == "0XFF");
    BOOST_TEST(convert<string>( 15, ccnv, "bad") ==  "0XF");
    //]
    //[stream_example4
    ccnv(arg::base = cnv::base::dec)
        (arg::uppercase = false)
        (arg::notation = cnv::notation::scientific);
    //]
    //[stream_example5
    ccnv(std::dec)(std::uppercase)(std::scientific);
    //]
}

static
void
example6()
{
    //[stream_example6
    boost::cnv::cstream      cnv1;
    boost::cnv::lexical_cast cnv2;

    change chg = change::up;
    string  s1 = convert<string>(chg, cnv1, "bad");                // Input type (change) deduced
    string  s2 = convert<string, change>(change::dn, cnv1, "bad"); // Input type (change) enforced

    BOOST_TEST(convert<change>("up", cnv1, change::no) == change::up);
    BOOST_TEST(convert<change>("up", cnv2, change::no) == change::up);
    BOOST_TEST(s1 == "up");
    BOOST_TEST(s2 == "dn");
    //]
}

int
main(int, char const* [])
{
    example1();
    example2();
    example6();

    return boost::report_errors();
}
