// Boost.Convert test and usage example
// Copyright (c) 2009-2014 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#include "./test.hpp"

#ifdef BOOST_CONVERT_INTEL_SFINAE_BROKEN
int main(int, char const* []) { return 0; }
#else

#include <boost/convert.hpp>
#include <boost/convert/printf.hpp>
#include <boost/convert/stream.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/uniform_01.hpp>
#include <boost/lexical_cast.hpp>

//[strtol_basic_deployment_header
#include <boost/convert.hpp>
#include <boost/convert/strtol.hpp>

using std::string;
using std::wstring;
using boost::convert;

struct boost::cnv::by_default : public boost::cnv::strtol {};
//]

static
void
test_str_to_uint()
{
    string const         bad_str = "not an int";
    string const         neg_str = "-11";
    string const         std_str = "11";
    char const* const      c_str = "12";
    unsigned int const      imax = (std::numeric_limits<unsigned int>::max)();
    unsigned long int const lmax = (std::numeric_limits<unsigned long int>::max)();
    std::string const   imax_str = boost::lexical_cast<std::string>(imax);
    std::string const   lmax_str = boost::lexical_cast<std::string>(lmax);

    BOOST_TEST( 0 == convert<unsigned int>(bad_str).value_or(0));
    BOOST_TEST( 0 == convert<unsigned int>(neg_str).value_or(0));
    BOOST_TEST( 0 == convert<unsigned long int>(neg_str).value_or(0));
    BOOST_TEST( 0 == convert<unsigned long int>(neg_str).value_or(0));
    BOOST_TEST(11 == convert<unsigned int>(std_str).value());
    BOOST_TEST(12 == convert<unsigned int>(  c_str).value());
    BOOST_TEST(11 == convert<unsigned long int>(std_str).value());
    BOOST_TEST(12 == convert<unsigned long int>(  c_str).value());
    BOOST_TEST(imax == convert<     unsigned int>(imax_str).value());
    BOOST_TEST(lmax == convert<unsigned long int>(lmax_str).value());
}

static
void
test_str_to_int()
{
    //[strtol_basic_deployment
    string const    bad_str = "not an int";
    string const    std_str = "-11";
    char const* const c_str = "-12";

    BOOST_TEST( -1 == convert<int>(bad_str).value_or(-1));
    BOOST_TEST(-11 == convert<int>(std_str).value());
    BOOST_TEST(-12 == convert<int>(  c_str).value());
    //]
    wstring const      bad_wstr = L"not an int";
    wstring const      wstd_str = L"-11";
    wchar_t const* const wc_str = L"-12";

    BOOST_TEST( -1 == convert<int>(bad_wstr).value_or(-1));
    BOOST_TEST(-11 == convert<int>(wstd_str).value());
    BOOST_TEST(-12 == convert<int>(  wc_str).value());
}

static
void
test_int_to_str()
{
    short const      s_int = -123;
    int const        i_int = -123;
    long const       l_int = -123;
    long long const ll_int = -123;

    BOOST_TEST( "-123" == convert< std::string> ( s_int).value());
    BOOST_TEST( "-123" == convert< std::string> ( i_int).value());
    BOOST_TEST( "-123" == convert< std::string> ( l_int).value());
    BOOST_TEST( "-123" == convert< std::string> (ll_int).value());

    BOOST_TEST(L"-123" == convert<std::wstring> ( s_int).value());
    BOOST_TEST(L"-123" == convert<std::wstring> ( i_int).value());
    BOOST_TEST(L"-123" == convert<std::wstring> ( l_int).value());
    BOOST_TEST(L"-123" == convert<std::wstring> (ll_int).value());

    int const            imin = std::numeric_limits<int>::min();
    int const            imax = std::numeric_limits<int>::max();
    long int const       lmin = std::numeric_limits<long int>::min();
    long int const       lmax = std::numeric_limits<long int>::max();
    long long int const llmin = std::numeric_limits<long long int>::min();
    long long int const llmax = std::numeric_limits<long long int>::max();

    std::string const  imin_str = boost::lexical_cast<std::string>(imin);
    std::string const  imax_str = boost::lexical_cast<std::string>(imax);
    std::string const  lmin_str = boost::lexical_cast<std::string>(lmin);
    std::string const  lmax_str = boost::lexical_cast<std::string>(lmax);
    std::string const llmin_str = boost::lexical_cast<std::string>(llmin);
    std::string const llmax_str = boost::lexical_cast<std::string>(llmax);

    BOOST_TEST( imin_str == convert<std::string>( imin).value());
    BOOST_TEST( imax_str == convert<std::string>( imax).value());
    BOOST_TEST( lmin_str == convert<std::string>( lmin).value());
    BOOST_TEST( lmax_str == convert<std::string>( lmax).value());
    BOOST_TEST(llmin_str == convert<std::string>(llmin).value());
    BOOST_TEST(llmax_str == convert<std::string>(llmax).value());
}

static
void
test_uint_to_str()
{
    unsigned short const      us_int = 123;
    unsigned int const        ui_int = 123;
    unsigned long const       ul_int = 123;
    unsigned long long const ull_int = 123;

    BOOST_TEST( "123" == convert< std::string> ( us_int).value());
    BOOST_TEST( "123" == convert< std::string> ( ui_int).value());
    BOOST_TEST( "123" == convert< std::string> ( ul_int).value());
    BOOST_TEST( "123" == convert< std::string> (ull_int).value());

    BOOST_TEST(L"123" == convert<std::wstring> ( us_int).value());
    BOOST_TEST(L"123" == convert<std::wstring> ( ui_int).value());
    BOOST_TEST(L"123" == convert<std::wstring> ( ul_int).value());
    BOOST_TEST(L"123" == convert<std::wstring> (ull_int).value());

    unsigned int const            uimax = (std::numeric_limits<unsigned int>::max)();
    unsigned long int const       ulmax = (std::numeric_limits<unsigned long int>::max)();
    unsigned long long int const ullmax = (std::numeric_limits<unsigned long long int>::max)();

    std::string const  uimax_str = boost::lexical_cast<std::string>( uimax);
    std::string const  ulmax_str = boost::lexical_cast<std::string>( ulmax);
    std::string const ullmax_str = boost::lexical_cast<std::string>(ullmax);

    BOOST_TEST( uimax_str == convert<std::string>( uimax).value());
    BOOST_TEST( ulmax_str == convert<std::string>( ulmax).value());
    BOOST_TEST(ullmax_str == convert<std::string>(ullmax).value());
}

//[strtol_numeric_base_header
#include <boost/convert.hpp>
#include <boost/convert/strtol.hpp>

using std::string;
using std::wstring;
using boost::convert;

namespace cnv = boost::cnv;
namespace arg = boost::cnv::parameter;
//]
static
void
test_width()
{
    //[strtol_width
    boost::cnv::strtol cnv;

    string s01 = convert<string>( 12, cnv(arg::width = 4)).value();
    string s02 = convert<string>( 12, cnv(arg::width = 5)
                                         (arg::fill = '*')).value();
    string s03 = convert<string>( 12, cnv(arg::width = 5)
                                         (arg::fill = 'x')
                                         (arg::adjust = cnv::adjust::left)).value();
    string s04 = convert<string>(-98, cnv(arg::width = 6)
                                         (arg::fill = 'Z')
                                         (arg::adjust = cnv::adjust::right)).value();

    string s05 = convert<string>(-12.3451, cnv(arg::precision = 2)
                                              (arg::width = 10)
                                              (arg::fill = '*')
                                              (arg::adjust = cnv::adjust::left)).value();
    string s06 = convert<string>(-12.3450, cnv(arg::adjust = cnv::adjust::right)).value();
    string s07 = convert<string>(-12.3450, cnv(arg::adjust = cnv::adjust::center)).value();

    BOOST_TEST(s01 == "  12");
    BOOST_TEST(s02 == "***12");
    BOOST_TEST(s03 == "12xxx");
    BOOST_TEST(s04 == "ZZZ-98");
    BOOST_TEST(s05 == "-12.35****");
    BOOST_TEST(s06 == "****-12.35");
    BOOST_TEST(s07 == "**-12.35**");
    //]
}

static
void
test_base()
{
    //[strtol_numeric_base
    boost::cnv::strtol cnv;

    BOOST_TEST( "11111110" == convert< string>(254, cnv(arg::base = cnv::base::bin)).value());
    BOOST_TEST(      "254" == convert< string>(254, cnv(arg::base = cnv::base::dec)).value());
    BOOST_TEST(       "FE" == convert< string>(254, cnv(arg::base = cnv::base::hex)).value());
    BOOST_TEST(      "376" == convert< string>(254, cnv(arg::base = cnv::base::oct)).value());
    //]
    //[wide_strtol_numeric_base
    BOOST_TEST(L"11111110" == convert<wstring>(254, cnv(arg::base = cnv::base::bin)).value());
    BOOST_TEST(     L"254" == convert<wstring>(254, cnv(arg::base = cnv::base::dec)).value());
    BOOST_TEST(      L"FE" == convert<wstring>(254, cnv(arg::base = cnv::base::hex)).value());
    BOOST_TEST(     L"376" == convert<wstring>(254, cnv(arg::base = cnv::base::oct)).value());
    //]
}

static
void
test_skipws()
{
    //[strtol_skipws
    boost::cnv::strtol cnv;

    BOOST_TEST(-1 == convert<int>( " 12", cnv(arg::skipws = false)).value_or(-1));
    BOOST_TEST(12 == convert<int>( " 12", cnv(arg::skipws =  true)).value_or(-1));
    //]
    //[wide_strtol_skipws
    BOOST_TEST(-1 == convert<int>(L" 12", cnv(arg::skipws = false)).value_or(-1));
    BOOST_TEST(12 == convert<int>(L" 12", cnv(arg::skipws =  true)).value_or(-1));
    //]
}

static
void
dbl_to_str_example()
{
    //[strtol_precision
    boost::cnv::strtol cnv;

    BOOST_TEST(  "12.3" == convert<string>(12.3456, cnv(arg::precision = 1)).value());
    BOOST_TEST( "12.35" == convert<string>(12.3456, cnv(arg::precision = 2)).value());
    BOOST_TEST("12.346" == convert<string>(12.3456, cnv(arg::precision = 3)).value());

    BOOST_TEST(  "-12.3" == convert<string>(-12.3456, cnv(arg::precision = 1)).value());
    BOOST_TEST( "-12.35" == convert<string>(-12.3456, cnv(arg::precision = 2)).value());
    BOOST_TEST("-12.346" == convert<string>(-12.3456, cnv(arg::precision = 3)).value());
    //]
}

static
std::pair<double, int>
get_random()
{
    static boost::random::mt19937                          gen (::time(0));
    static boost::random::uniform_int_distribution<> precision (0, 6);
    static boost::random::uniform_int_distribution<>  int_part (0, SHRT_MAX);
    static boost::random::uniform_01<double>          fraction; // uniform double in [0,1)
    static bool                                           sign;

    double dbl = (int_part(gen) + fraction(gen)) * ((sign = !sign) ? 1 : -1);

//  printf("%.12f\n", dbl);

    return std::make_pair(dbl, precision(gen));
}

static
void
compare(std::pair<double, int> pair)
{
    boost::cnv::strtol cnv1;
    boost::cnv::printf cnv2;

    string s1 = convert<string>(pair.first, cnv1(arg::precision = pair.second)).value();
    string s2 = convert<string>(pair.first, cnv2(arg::precision = pair.second)).value();

    if (s1 != s2)
        printf("dbl=%.12f(%d).strtol/printf=%s/%s.\n", pair.first, pair.second, s1.c_str(), s2.c_str());
}

static
void
test_dbl_to_str()
{
//    double      round_up_abs01 = ::rint(-0.5);
//    double      round_up_abs02 = ::round(-0.5);
//    double      round_up_abs11 = ::rint(0.5);
//    double      round_up_abs12 = ::round(0.5);

//    double huge_v = 987654321098765432109.123;
//
//    printf("%f\n", huge_v);
//    string huge = convert<string>(huge_v, cnv1(arg::precision = 2)).value();
//    printf("%s\n", huge.c_str());

    int const num_tries = 1000000;
    double const dbls[] = { 0.90, 1.0, 1.1, 0.94, 0.96, 1.04, 1.05, 1.06, 9.654, 999.888 };
    int const  num_dbls = sizeof(dbls) / sizeof(dbls[0]);

    printf("cnv::strtol::%s: started with %d random numbers...\n", __FUNCTION__, num_tries);

    BOOST_TEST(   "0" == convert<string>( 0.0, cnv::strtol()(arg::precision = 0)).value());
    BOOST_TEST( "0.0" == convert<string>( 0.0, cnv::strtol()(arg::precision = 1)).value());
    BOOST_TEST("0.00" == convert<string>( 0.0, cnv::strtol()(arg::precision = 2)).value());
    BOOST_TEST(   "1" == convert<string>(0.95, cnv::strtol()(arg::precision = 0)).value());
    BOOST_TEST( "1.0" == convert<string>(0.95, cnv::strtol()(arg::precision = 1)).value());
    BOOST_TEST("0.95" == convert<string>(0.95, cnv::strtol()(arg::precision = 2)).value());

    for (int k = 0; k < num_tries; ++k)
        compare(get_random());

    for (int k = 0; k < num_dbls; ++k)
        for (int precision = 0; precision < 3; ++precision)
        {
            compare(std::make_pair( dbls[k], precision));
            compare(std::make_pair(-dbls[k], precision));
        }

    printf("cnv::strtol::%s: finished.\n", __FUNCTION__);
}

static
void
test_user_string()
{
    //[strtol_user_string
    boost::cnv::strtol cnv;

    BOOST_TEST(  "12" == convert<my_string>(12, cnv).value());
    BOOST_TEST("0.95" == convert<my_string>(0.95, cnv(arg::precision = 2)).value());
    //]
}

static
void
test_user_type()
{
    //[strtol_user_type
    boost::cnv::strtol cnv;
    change          up_chg = change::up;
    change          dn_chg = change::dn;

    BOOST_TEST(convert<std::string>(up_chg, cnv, "bad") == "up");
    BOOST_TEST(convert<std::string>(dn_chg, cnv, "bad") == "dn");
    BOOST_TEST(convert<std::string>(    12, cnv, "bad") == "12");

    BOOST_TEST(convert<change>("up", cnv, change::no) == change::up);
    BOOST_TEST(convert<change>("dn", cnv, change::no) == change::dn);
    BOOST_TEST(convert<   int>("12", cnv, -1) == 12);
    //]
}

int
main(int, char const* [])
{
    dbl_to_str_example();

    test_str_to_int();
    test_str_to_uint();
    test_int_to_str();
    test_uint_to_str();
    test_base();
    test_skipws();
    test_dbl_to_str();
    test_width();
    test_user_string();
    test_user_type();

    return boost::report_errors();
}

#endif
