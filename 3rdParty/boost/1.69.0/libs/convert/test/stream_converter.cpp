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
#include <boost/detail/lightweight_test.hpp>
#include <cstdio>
#include <cstdlib>
#include <stdlib.h>

//[stream_using
using std::string;
using std::wstring;
using boost::convert;
//]
//[stream_cnv_namespace_shortcut
namespace cnv = boost::cnv;
namespace arg = boost::cnv::parameter;
//]

static
void
test_dbl_to_str()
{
    boost::cnv::cstream cnv;

    cnv(std::fixed);

    BOOST_TEST(convert<string>( 99.999, cnv(arg::precision = 2)).value_or("bad") == "100.00");
    BOOST_TEST(convert<string>( 99.949, cnv(arg::precision = 2)).value_or("bad") ==  "99.95");
    BOOST_TEST(convert<string>(-99.949, cnv(arg::precision = 2)).value_or("bad") == "-99.95");
    BOOST_TEST(convert<string>( 99.949, cnv(arg::precision = 1)).value_or("bad") ==   "99.9");
    BOOST_TEST(convert<string>(  0.999, cnv(arg::precision = 2)).value_or("bad") ==   "1.00");
    BOOST_TEST(convert<string>( -0.999, cnv(arg::precision = 2)).value_or("bad") ==  "-1.00");
    BOOST_TEST(convert<string>(  0.949, cnv(arg::precision = 2)).value_or("bad") ==   "0.95");
    BOOST_TEST(convert<string>( -0.949, cnv(arg::precision = 2)).value_or("bad") ==  "-0.95");
    BOOST_TEST(convert<string>(  1.949, cnv(arg::precision = 1)).value_or("bad") ==    "1.9");
    BOOST_TEST(convert<string>( -1.949, cnv(arg::precision = 1)).value_or("bad") ==   "-1.9");
}

static
void
test_numbase()
{
    //[stream_numbase_example1
    /*`The following example demonstrates the deployment of `std::dec`, `std::oct` `std::hex`
       manipulators:
     */
    boost::cnv::cstream ccnv;

    BOOST_TEST(convert<int>( "11", ccnv(std::hex)).value_or(0) == 17); // 11(16) = 17(10)
    BOOST_TEST(convert<int>( "11", ccnv(std::oct)).value_or(0) ==  9); // 11(8)  = 9(10)
    BOOST_TEST(convert<int>( "11", ccnv(std::dec)).value_or(0) == 11);

    BOOST_TEST(convert<string>( 18, ccnv(std::hex)).value_or("bad") == "12"); // 18(10) = 12(16)
    BOOST_TEST(convert<string>( 10, ccnv(std::oct)).value_or("bad") == "12"); // 10(10) = 12(8)
    BOOST_TEST(convert<string>( 12, ccnv(std::dec)).value_or("bad") == "12");
    BOOST_TEST(convert<string>(255, ccnv(arg::base = boost::cnv::base::oct)).value_or("bad") == "377");
    BOOST_TEST(convert<string>(255, ccnv(arg::base = boost::cnv::base::hex)).value_or("bad") ==  "ff");
    BOOST_TEST(convert<string>(255, ccnv(arg::base = boost::cnv::base::dec)).value_or("bad") == "255");

    ccnv(std::showbase);

    BOOST_TEST(convert<string>(18, ccnv(std::hex)).value_or("bad") == "0x12");
    BOOST_TEST(convert<string>(10, ccnv(std::oct)).value_or("bad") ==  "012");

    ccnv(std::uppercase);

    BOOST_TEST(convert<string>(18, ccnv(std::hex)).value_or("bad") == "0X12");
    //]
    //[stream_numbase_example2
    BOOST_TEST(convert<int>("11", ccnv(arg::base = cnv::base::hex)).value_or(0) == 17);
    BOOST_TEST(convert<int>("11", ccnv(arg::base = cnv::base::oct)).value_or(0) ==  9);
    BOOST_TEST(convert<int>("11", ccnv(arg::base = cnv::base::dec)).value_or(0) == 11);
    //]
    //[wide_stream_numeric_base
    boost::cnv::wstream wcnv;

    BOOST_TEST(convert<int>(L"11", wcnv(std::hex)).value_or(0) == 17); // 11(16) = 17(10)
    BOOST_TEST(convert<int>(L"11", wcnv(std::oct)).value_or(0) ==  9); // 11(8)  = 9(10)
    BOOST_TEST(convert<int>(L"11", wcnv(std::dec)).value_or(0) == 11);

    BOOST_TEST(convert<wstring>(254, wcnv(arg::base = cnv::base::dec)).value_or(L"bad") == L"254");
    BOOST_TEST(convert<wstring>(254, wcnv(arg::base = cnv::base::hex)).value_or(L"bad") ==  L"fe");
    BOOST_TEST(convert<wstring>(254, wcnv(arg::base = cnv::base::oct)).value_or(L"bad") == L"376");
    //]
}

static
void
test_boolalpha()
{
    boost::cnv::cstream cnv;
    //[stream_boolalpha_example
    BOOST_TEST(convert<string>( true, cnv(std::boolalpha)).value_or("bad") ==  "true");
    BOOST_TEST(convert<string>(false, cnv(std::boolalpha)).value_or("bad") == "false");

    BOOST_TEST(convert<bool>( "true", cnv(std::boolalpha)).value_or(false) ==  true);
    BOOST_TEST(convert<bool>("false", cnv(std::boolalpha)).value_or( true) == false);

    BOOST_TEST(convert<string>( true, cnv(std::noboolalpha)).value_or("bad") == "1");
    BOOST_TEST(convert<string>(false, cnv(std::noboolalpha)).value_or("bad") == "0");

    BOOST_TEST(convert<bool>("1", cnv(std::noboolalpha)).value_or(false) ==  true);
    BOOST_TEST(convert<bool>("0", cnv(std::noboolalpha)).value_or( true) == false);
    //]
}

static
void
test_skipws_char()
{
    //[stream_skipws_example
    boost::cnv::cstream    ccnv;
    char const* const cstr_good = "  123";
    char const* const  cstr_bad = "  123 "; // std::skipws only affects leading spaces.

    ccnv(std::skipws);        // Ignore leading whitespaces
//  ccnv(arg::skipws = true); // Ignore leading whitespaces. Alternative interface

    BOOST_TEST(convert<int>(cstr_good, ccnv).value_or(0) == 123);
    BOOST_TEST(convert<string>("  123", ccnv).value_or("bad") == "123");

    BOOST_TEST(!convert<int>(cstr_bad, ccnv));

    ccnv(std::noskipws);       // Do not ignore leading whitespaces
//  ccnv(arg::skipws = false); // Do not ignore leading whitespaces. Alternative interface

    // All conversions fail.
    BOOST_TEST(!convert<int>(cstr_good, ccnv));
    BOOST_TEST(!convert<int>( cstr_bad, ccnv));
    //]
}

static
void
test_skipws_wchar()
{
    //[wide_stream_skipws
    boost::cnv::wstream wcnv;

    wcnv(std::noskipws); // Do not ignore leading whitespaces

    BOOST_TEST( convert<int>(   L"123", wcnv).value_or(0) == 123);
    BOOST_TEST(!convert<int>( L"  123", wcnv));
    BOOST_TEST(!convert<int>(L"  123 ", wcnv));

    wcnv(std::skipws);        // Ignore leading whitespaces
//  wcnv(arg::skipws = true); // Ignore leading whitespaces. Alternative interface

    BOOST_TEST( convert<int>( L"  123", wcnv).value_or(0) == 123);
    BOOST_TEST(!convert<int>(L"  123 ", wcnv));
    //]
}

static
void
test_width()
{
    //[stream_width_example
    boost::cnv::cstream cnv;

    boost::optional<string> s01 = convert<string>(12, cnv(std::setw(4)));
    boost::optional<string> s02 = convert<string>(12, cnv(std::setw(5))(std::setfill('*')));
    boost::optional<string> s03 = convert<string>(12, cnv(std::setw(5))(std::setfill('*'))(std::left));

    BOOST_TEST(s01 && s01.value() == "  12");  // Field width = 4.
    BOOST_TEST(s02 && s02.value() == "***12"); // Field width = 5, filler = '*'.
    BOOST_TEST(s03 && s03.value() == "12***"); // Field width = 5, filler = '*', left adjustment

    /*`It needs to be remembered that `boost::cnv::stream` converter uses `std::stream` as its underlying
       conversion engine. Consequently, formatting-related behavior are driven by the `std::stream`. Namely,
       after every operation is performed, the ['default field width is restored]. The values of
       the fill character and the adjustment remain unchanged until they are modified explicitly.
     */

    // The fill and adjustment remain '*' and 'left'.
    boost::optional<string> s11 = convert<string>(12, cnv(arg::width = 4));
    boost::optional<string> s12 = convert<string>(12, cnv(arg::width = 5)
                                                         (arg::fill = ' ')
                                                         (arg::adjust = cnv::adjust::right));

    BOOST_TEST(s11 && s11.value() == "12**");  // Field width was set to 4.
    BOOST_TEST(s12 && s12.value() == "   12"); // Field width was set to 5 with the ' ' filler.
    //]
}

static
void
test_manipulators()
{
    boost::cnv::cstream ccnv;
    boost::cnv::wstream wcnv;

    int const hex_v01 = boost::convert<int>("FF", ccnv(std::hex)).value_or(0);
    int const hex_v02 = boost::convert<int>(L"F", wcnv(std::hex)).value_or(0);
    int const hex_v03 = boost::convert<int>("FF", ccnv(std::dec)).value_or(-5);
    int const hex_v04 = boost::convert<int>(L"F", wcnv(std::dec)).value_or(-6);

    BOOST_TEST(hex_v01 == 255); // "FF"
    BOOST_TEST(hex_v02 ==  15); // L"F"
    BOOST_TEST(hex_v03 ==  -5); // Failed conversion
    BOOST_TEST(hex_v04 ==  -6); // Failed conversion

    ccnv(std::noshowbase)(std::nouppercase)(std::oct);

    BOOST_TEST(boost::convert<string>(255, ccnv).value_or("bad") == "377");
    BOOST_TEST(boost::convert<string>( 15, ccnv).value_or("bad") ==  "17");

    ccnv(std::showbase);

    BOOST_TEST(boost::convert<string>(255, ccnv).value_or("bad") == "0377");
    BOOST_TEST(boost::convert<string>( 15, ccnv).value_or("bad") ==  "017");

    ccnv(std::uppercase)(std::hex);

    BOOST_TEST(boost::convert<string>(255, ccnv).value_or("bad") == "0XFF");
    BOOST_TEST(boost::convert<string>( 15, ccnv).value_or("bad") ==  "0XF");

    ccnv(std::noshowbase)(std::nouppercase)(std::oct);

    BOOST_TEST(boost::convert<string>(255, ccnv).value_or("bad") == "377");
    BOOST_TEST(boost::convert<string>( 15, ccnv).value_or("bad") ==  "17");

    ccnv(std::showbase)(arg::uppercase = true)(arg::base = cnv::base::hex);

    BOOST_TEST(boost::convert<string>(255, ccnv).value_or("bad") == "0XFF");
    BOOST_TEST(boost::convert<string>( 15, ccnv).value_or("bad") ==  "0XF");
}

void
test_locale_example()
{
    //[stream_locale_example1
    boost::cnv::cstream cnv;
    std::locale  rus_locale;
    std::locale  eng_locale;

    char const* eng_locale_name = test::cnv::is_msc ? "English_United States.1251" : "en_US.UTF-8";
    char const* rus_locale_name = test::cnv::is_msc ? "Russian_Russia.1251" : "ru_RU.UTF-8";
    char const*    rus_expected = test::cnv::is_msc ? "1,235e-002" : "1,235e-02";
    char const*    eng_expected = test::cnv::is_msc ? "1.235e-002" : "1.235e-02";
    char const*      double_s01 = test::cnv::is_msc ? "1.2345E-002" : "1.2345E-02";

//  cnv(std::setprecision(4))(std::uppercase)(std::scientific);
    cnv(arg::precision = 4)
       (arg::uppercase = true)
       (arg::notation = cnv::notation::scientific);

    double double_v01 = convert<double>(double_s01, cnv).value_or(0);
    string double_s02 = convert<string>(double_v01, cnv).value_or("bad");

    BOOST_TEST(double_s01 == double_s02);

    try { rus_locale = std::locale(rus_locale_name); }
    catch (...) { printf("Bad locale %s.\n", rus_locale_name); exit(1); }

    try { eng_locale = std::locale(eng_locale_name); }
    catch (...) { printf("Bad locale %s.\n", eng_locale_name); exit(1); }

//  cnv(std::setprecision(3))(std::nouppercase);
    cnv(arg::precision = 3)(arg::uppercase = false);

    string double_rus = convert<string>(double_v01, cnv(rus_locale)).value_or("bad double_rus");
    string double_eng = convert<string>(double_v01, cnv(eng_locale)).value_or("bad double_eng");

    BOOST_TEST(double_rus == rus_expected);
    BOOST_TEST(double_eng == eng_expected);
    //]
}

void
test_locale(double v, boost::cnv::cstream const& cnv, char const* expected)
{
    boost::optional<string> res = convert<string>(v, cnv);
    std::string             str = res ? *res : "conversion failed";

    BOOST_TEST(res);
    BOOST_TEST(str == expected);

    if (str != expected)
        printf("%s [%d]: result=<%s>, expected=<%s>\n", __FILE__, __LINE__, str.c_str(), expected);
}

static
void
test_locale()
{
    boost::cnv::cstream     cnv;
    std::locale      rus_locale;
    std::locale      eng_locale;
    bool             eng_ignore = false;
    bool             rus_ignore = false;
    char const* eng_locale_name = test::cnv::is_msc ? "English_United States.1251" : "en_US.UTF-8";
    char const* rus_locale_name = test::cnv::is_msc ? "Russian_Russia.1251" : "ru_RU.UTF-8";
    char const*    eng_expected = test::cnv::is_old_msc ? "1.235e-002" : "1.235e-02";
    char const*    rus_expected = test::cnv::is_old_msc ? "1,235e-002" : "1,235e-02";
    char const*      double_s01 = test::cnv::is_old_msc ? "1.2345E-002" : "1.2345E-02";

    cnv(arg::precision = 4)
       (arg::uppercase = true)
       (arg::notation = cnv::notation::scientific);

    double const double_v01 = convert<double>(double_s01, cnv).value_or(0);
    string const double_s02 = convert<string>(double_v01, cnv).value_or("bad");

    BOOST_TEST(double_v01 != 0);
    BOOST_TEST(double_s01 == double_s02);

    if (double_s01 != double_s02)
        printf("%s [%d]: <%s> != <%s>\n", __FILE__, __LINE__, double_s01, double_s02.c_str());

    try { eng_locale = std::locale(eng_locale_name); }
    catch (...) { printf("Bad locale %s. Ignored.\n", eng_locale_name); eng_ignore = true; }

    try { rus_locale = std::locale(rus_locale_name); }
    catch (...) { printf("Bad locale %s. Ignored.\n", rus_locale_name); rus_ignore = true; }

//  cnv(std::setprecision(3))(std::nouppercase);
    cnv(arg::precision = 3)(arg::uppercase = false);

    if (!eng_ignore) test_locale(double_v01, cnv(eng_locale), eng_expected);
    if (!rus_ignore) test_locale(double_v01, cnv(rus_locale), rus_expected);
}

static
void
test_user_str()
{
    //[stream_my_string
    boost::cnv::cstream cnv;
    my_string        my_str("123");

    cnv(std::setprecision(2))(std::fixed);

    BOOST_TEST(convert<int>(my_str, cnv).value_or(0) == 123);

    BOOST_TEST(convert<my_string>( 99.999, cnv).value_or("bad") == "100.00");
    BOOST_TEST(convert<my_string>( 99.949, cnv).value_or("bad") ==  "99.95");
    BOOST_TEST(convert<my_string>(-99.949, cnv).value_or("bad") == "-99.95");
    //]
}

int
main(int, char const* [])
{
    try
    {
        // QNX fails to handle std::skipws for wchat_t.
        // Excluding from tests so that I do not have to stare on the yellow box (regression failure)
        /*********************/ test_skipws_char();
        if (!test::cnv::is_qnx) test_skipws_wchar();

        test_numbase();
        test_boolalpha();
        test_width();
        test_manipulators();
        test_locale();
        test_dbl_to_str();
        test_user_str();
    }
    catch(boost::bad_optional_access const&)
    {
        BOOST_TEST(!"Caught boost::bad_optional_access exception");
    }
    catch(...)
    {
        BOOST_TEST(!"Caught an unknown exception");
    }
    return boost::report_errors();
}

#endif
