// Boost.Convert test and usage example
// Copyright (c) 2009-2014 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#include "./test.hpp"

#ifdef BOOST_CONVERT_INTEL_SFINAE_BROKEN
int main(int, char const* []) { return 0; }
#else

#include <boost/convert.hpp>
#include <boost/convert/spirit.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <cstdio>

using std::string;
using std::wstring;
using boost::convert;

namespace cnv = boost::cnv;
namespace arg = boost::cnv::parameter;

struct boost::cnv::by_default : public boost::cnv::spirit {};

int
main(int, char const* [])
{
    char const* const     c_stri ("12345");
    char const* const     c_strd ("123.45");
    wchar_t const* const c_wstri (L"12345");
    wchar_t const* const c_wstrd (L"123.45");
    std::string const   std_stri (c_stri);
    std::string const   std_strd (c_strd);
    std::wstring const std_wstri (c_wstri);
    std::wstring const std_wstrd (c_wstrd);
    my_string const      my_stri (c_stri, c_stri + strlen(c_stri));
    my_string const      my_strd (c_strd, c_strd + strlen(c_strd));

    boost::cnv::spirit cnv;

    BOOST_TEST( 12345 == convert<     int>(   c_stri).value());
    BOOST_TEST( 12345 == convert<     int>(  c_wstri).value());
    BOOST_TEST( 12345 == convert<     int>( std_stri).value());
    BOOST_TEST( 12345 == convert<     int>(std_wstri).value());
    BOOST_TEST( 12345 == convert<     int>(  my_stri).value());
    BOOST_TEST( 12345 == convert<unsigned int>(c_stri).value());
    BOOST_TEST( 12345 == convert<long int>(   c_stri).value());
    BOOST_TEST( 12345 == convert<long int>(  c_wstri).value());
    BOOST_TEST( 12345 == convert<long int>( std_stri).value());
    BOOST_TEST( 12345 == convert<long int>(std_wstri).value());
    BOOST_TEST( 12345 == convert<long int>(  my_stri).value());
    BOOST_TEST( 12345 == convert<unsigned long int>(c_stri).value());
    BOOST_TEST( 12345 == convert<long long int>(c_stri).value());
    BOOST_TEST( 12345 == convert<unsigned long long int>(c_stri).value());
    BOOST_TEST(123.45 == convert<  double>(   c_strd).value());
    BOOST_TEST(123.45 == convert<  double>(  c_wstrd).value());
//  BOOST_TEST(123.45 == convert<  double>( std_strd).value());
//  BOOST_TEST(123.45 == convert<  double>(std_wstrd).value());
    BOOST_TEST(123.45 == convert<  double>(  my_strd).value());

    BOOST_TEST(!convert<   int>("uhm"));
    BOOST_TEST(!convert<   int>(L"uhm"));
    BOOST_TEST(!convert<double>("12.uhm"));
    BOOST_TEST(!convert<double>("L12.uhm"));

    BOOST_TEST(  "1234" == convert<string>(1234).value());
    BOOST_TEST(  "1234" == convert<string>(1234u).value());
    BOOST_TEST(  "1234" == convert<string>(1234ll).value());
    BOOST_TEST(  "1234" == convert<string>(1234ull).value());
    BOOST_TEST( L"1234" == convert<wstring>(1234).value());
    BOOST_TEST( "12xxx" == convert<string>(12, cnv(arg::width = 5)
                                                  (arg::fill = 'x')
                                                  (arg::adjust = cnv::adjust::left)).value());
    BOOST_TEST(L"12xxx" == convert<wstring>(12, cnv(arg::width = 5)
                                                   (arg::fill = 'x')
                                                   (arg::adjust = cnv::adjust::left)).value());
    BOOST_TEST( "x12xx" == convert<string>(12, cnv(arg::adjust = cnv::adjust::center)).value());
    BOOST_TEST(L"x12xx" == convert<wstring>(12, cnv(arg::adjust = cnv::adjust::center)).value());

//    BOOST_TEST("12.34" == convert<std::string>(12.34).value());
//    printf("%s\n", convert<std::string>(12.34).value().c_str());

    return boost::report_errors();
}

#endif
