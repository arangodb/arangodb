// Boost.Convert test and usage example
// Copyright (c) 2009-2016 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#include "./test.hpp"

#if defined(BOOST_CONVERT_IS_NOT_SUPPORTED)
int main(int, char const* []) { return 0; }
#else

#include <boost/convert.hpp>
#include <boost/convert/detail/is_converter.hpp>
#include <boost/detail/lightweight_test.hpp>

using std::string;
using std::wstring;

namespace { namespace local
{
    typedef void not_converter01 (string const&);
    typedef void not_converter02 (int const&, string const&);
    typedef void not_converter03 ();
    typedef void     converter01 (int const&, boost::optional<string>&);
    typedef void     converter02 (double const&, boost::optional<string>&);

    struct converter11
    {
        void operator()(int const&, boost::optional<string>&) {}
        void operator()(double const&, boost::optional<string>&) {}
        void operator()(string const&, boost::optional<int>&) {}
    };
    struct converter12
    {
        template<typename TypeIn, typename TypeOut> void operator()(TypeIn const&, boost::optional<TypeOut>&) {}
    };
    struct converter13
    {
        void operator()(int const&, boost::optional<string>&) {}
    };
    struct converter14
    {
        void operator()(int const&, boost::optional<string>&) const {}
    };
    struct not_converter11
    {
    };
    struct not_converter12
    {
        void operator()() {}
    };
    struct not_converter13
    {
        void operator()(int const&, string const&) {}
    };
}}

int
main(int, char const* [])
{
    BOOST_TEST( (boost::cnv::is_cnv<local::converter01, int, string>::value));
    BOOST_TEST( (boost::cnv::is_cnv<local::converter01, double, string>::value));
    BOOST_TEST( (boost::cnv::is_cnv<local::converter01, short int, string>::value));
    BOOST_TEST( (boost::cnv::is_cnv<local::converter11, int, string>::value));
    BOOST_TEST( (boost::cnv::is_cnv<local::converter11, double, string>::value));
    BOOST_TEST( (boost::cnv::is_cnv<local::converter11, string, int>::value));
    BOOST_TEST( (boost::cnv::is_cnv<local::converter12, int, string>::value));
    BOOST_TEST( (boost::cnv::is_cnv<local::converter12, double, string>::value));
    BOOST_TEST( (boost::cnv::is_cnv<local::converter12, string, int>::value));
    BOOST_TEST( (boost::cnv::is_cnv<local::converter13, int, string>::value));
    BOOST_TEST( (boost::cnv::is_cnv<local::converter13, double, string>::value));
    BOOST_TEST( (boost::cnv::is_cnv<local::converter13, int, string>::value));
    BOOST_TEST( (boost::cnv::is_cnv<local::converter14, int, string>::value));
    BOOST_TEST(!(boost::cnv::is_cnv<local::converter13, string, int>::value));
    BOOST_TEST(!(boost::cnv::is_cnv<local::converter14, string, int>::value));

    BOOST_TEST(!(boost::cnv::is_cnv<local::not_converter01, int, string>::value));
    BOOST_TEST(!(boost::cnv::is_cnv<local::not_converter02, int, string>::value));
    BOOST_TEST(!(boost::cnv::is_cnv<local::not_converter11, int, string>::value));
    BOOST_TEST(!(boost::cnv::is_cnv<local::not_converter12, int, string>::value));
    BOOST_TEST(!(boost::cnv::is_cnv<local::not_converter13, int, string>::value));

    return boost::report_errors();
}

#endif
