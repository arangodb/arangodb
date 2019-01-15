// Boost.Convert test and usage example
// Copyright (c) 2009-2016 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#include "./test.hpp"

#if defined(BOOST_CONVERT_IS_NOT_SUPPORTED)
int main(int, char const* []) { return 0; }
#else

#include <boost/convert.hpp>
#include <boost/convert/stream.hpp>
#include <boost/convert/printf.hpp>
#include <boost/convert/strtol.hpp>
#include <boost/convert/lexical_cast.hpp>
#include <boost/convert/spirit.hpp>
#include <boost/detail/lightweight_test.hpp>

using std::string;

template<typename Converter>
void
int_to_str(Converter const& cnv)
{
    string const    not_int_str = "not an int";
    string const        std_str = "-11";
    char const* const     c_str = "-12";
    char const      array_str[] = "-15";

    ////////////////////////////////////////////////////////////////////////////
    // Testing int-to-string conversion with various string
    // containers as the fallback values.
    ////////////////////////////////////////////////////////////////////////////

    string                    s01 = boost::convert< string>(-1, cnv).value_or(std_str);
    string                    s02 = boost::convert< string>(-2, cnv).value_or(c_str);
    string                    s05 = boost::convert< string>(-5, cnv).value_or(array_str);
    boost::optional< string> rs01 = boost::convert< string>(-1, cnv);
    boost::optional< string> rs02 = boost::convert< string>(-2, cnv);
    boost::optional< string> rs05 = boost::convert< string>(-5, cnv);

    BOOST_TEST(s01 ==  "-1"); BOOST_TEST(rs01 && rs01.value() ==  "-1");
    BOOST_TEST(s02 ==  "-2"); BOOST_TEST(rs02 && rs02.value() ==  "-2");
    BOOST_TEST(s05 ==  "-5"); BOOST_TEST(rs05 && rs05.value() ==  "-5");

    string                    s11 = boost::convert< string>(-1, cnv).value();
    boost::optional< string> rs11 = boost::convert< string>(-1, cnv);

    BOOST_TEST( s11 ==  "-1");
    BOOST_TEST(rs11 && rs11.value() ==  "-1");
}

template<typename Converter>
void
str_to_int(Converter const& cnv)
{
    string const           not_int_str = "not an int";
    string const               std_str =  "-11";
    char const* const            c_str = "-123";
    char const             array_str[] = "3456";

    ////////////////////////////////////////////////////////////////////////////
    // Testing various kinds of string containers.
    // Testing implicit conversion directly to TypeOut (int).
    // Testing with the fallback value value provided.
    // On failure returns the provided fallback value and DOES NOT THROW.
    ////////////////////////////////////////////////////////////////////////////

    int const a00 = boost::convert<int>(not_int_str, cnv).value_or(-1);
    int const a01 = boost::convert<int>(std_str,     cnv).value_or(-1);
    int const a02 = boost::convert<int>(c_str,       cnv).value_or(-1);
    int const a05 = boost::convert<int>(array_str,   cnv).value_or(-1);

    BOOST_TEST(a00 ==   -1); // Failed conversion
    BOOST_TEST(a01 ==  -11);
    BOOST_TEST(a02 == -123);
    BOOST_TEST(a05 == 3456);

    ////////////////////////////////////////////////////////////////////////////
    // Testing "optional"
    ////////////////////////////////////////////////////////////////////////////

    boost::optional<int> const r00 = boost::convert<int>(not_int_str, cnv);
    boost::optional<int> const r01 = boost::convert<int>(std_str,     cnv);
    boost::optional<int> const r02 = boost::convert<int>(c_str,       cnv);
    boost::optional<int> const r05 = boost::convert<int>(array_str,   cnv);

    BOOST_TEST(!r00); // Failed conversion
    BOOST_TEST( r01 && r01.value() ==  -11);
    BOOST_TEST( r02 && r02.value() == -123);
    BOOST_TEST( r05 && r05.value() == 3456);

    ////////////////////////////////////////////////////////////////////////////
    // Testing value() on invalid result.
    ////////////////////////////////////////////////////////////////////////////

    try
    {
        boost::convert<int>(not_int_str, cnv).value();
        BOOST_TEST(!"failed to throw");
    }
    catch (std::exception&)
    {
        // Expected behavior: received 'boost::convert failed' exception. All well.
    }
    ////////////////////////////////////////////////////////////////////////////
    // Testing value() on valid result.
    ////////////////////////////////////////////////////////////////////////////

    int a21 = boost::convert<int>(std_str,   cnv).value();
    int a22 = boost::convert<int>(c_str,     cnv).value();
    int a25 = boost::convert<int>(array_str, cnv).value();

    BOOST_TEST(a21 ==  -11);
    BOOST_TEST(a22 == -123);
    BOOST_TEST(a25 == 3456);

    ////////////////////////////////////////////////////////////////////////////
    // Testing empty string.
    ////////////////////////////////////////////////////////////////////////////

    int a31 = boost::convert<int>(std::string(), cnv).value_or(-1);
    int a32 = boost::convert<int>(std::string(""), cnv).value_or(-1);
    int a33 = boost::convert<int>("", cnv).value_or(-1);

    BOOST_ASSERT(a31 == -1);
    BOOST_ASSERT(a32 == -1);
    BOOST_ASSERT(a33 == -1);
}

int
main(int, char const* [])
{
    boost::cnv::lexical_cast lxcast_cnv;
    boost::cnv::cstream      stream_cnv;
    boost::cnv::strtol       strtol_cnv;
    boost::cnv::printf       printf_cnv;
    boost::cnv::spirit       spirit_cnv;

    str_to_int(lxcast_cnv);
    str_to_int(stream_cnv);
    str_to_int(strtol_cnv);
    str_to_int(printf_cnv);
    str_to_int(spirit_cnv);

    int_to_str(lxcast_cnv);
    int_to_str(stream_cnv);
    int_to_str(strtol_cnv);
    int_to_str(printf_cnv);
    int_to_str(spirit_cnv);

    return boost::report_errors();
}

#endif
