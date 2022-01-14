// Boost.Convert test and usage example
// Copyright (c) 2009-2020 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#include "./test.hpp"

#if defined(BOOST_CONVERT_IS_NOT_SUPPORTED)
int main(int, char const* []) { return 0; }
#else

#include <boost/convert.hpp>
#include <boost/convert/stream.hpp>
#include <functional>

namespace { namespace local
{
    bool    called_functor_int;
    bool called_functor_double;
    bool    called_functor_foo;
    bool   called_function_int;
    bool  called_function_long;
}}

struct    functor_int { int    operator() () const { local::   called_functor_int = true; return INT_MAX; }};
struct functor_double { double operator() () const { local::called_functor_double = true; return INT_MAX; }};
struct    functor_foo { int       func (int) const { local::   called_functor_foo = true; return INT_MAX; }};

int   function_int () { local:: called_function_int = true; return INT_MAX; }
long function_long () { local::called_function_long = true; return INT_MAX; }

int
main(int, char const* [])
{
    auto cnv = boost::cnv::cstream();
    auto foo = functor_foo();

    int i01 = boost::convert<int>("uhm", cnv).value_or_eval(functor_int());
    int i02 = boost::convert<int>("uhm", cnv).value_or_eval(functor_double());
    int i03 = boost::convert<int>("uhm", cnv).value_or_eval(std::bind(&functor_foo::func, foo, 0));
    int i04 = boost::convert<int>("uhm", cnv).value_or_eval(function_int);
    int i05 = boost::convert<int>("uhm", cnv).value_or_eval(function_long);

    BOOST_TEST(local::   called_functor_int && i01 == INT_MAX);
    BOOST_TEST(local::called_functor_double && i02 == INT_MAX);
    BOOST_TEST(local::   called_functor_foo && i03 == INT_MAX);
    BOOST_TEST(local::  called_function_int && i04 == INT_MAX);
    BOOST_TEST(local:: called_function_long && i05 == INT_MAX);

    local::   called_functor_int = false;
    local::called_functor_double = false;
    local::   called_functor_foo = false;
    local::  called_function_int = false;
    local:: called_function_long = false;

    boost::convert<int>("uhm", cnv, functor_int());
    boost::convert<int>("uhm", cnv, functor_double());
    boost::convert<int>("uhm", cnv, std::bind(&functor_foo::func, foo, 0));
    boost::convert<int>("uhm", cnv, function_int);
    boost::convert<int>("uhm", cnv, function_long);

    BOOST_TEST(local::   called_functor_int && i01 == INT_MAX);
    BOOST_TEST(local::called_functor_double && i02 == INT_MAX);
    BOOST_TEST(local::   called_functor_foo && i03 == INT_MAX);
    BOOST_TEST(local::  called_function_int && i04 == INT_MAX);
    BOOST_TEST(local:: called_function_long && i05 == INT_MAX);

    try
    {
        boost::convert<int>("uhm", cnv, boost::throw_on_failure);
        BOOST_TEST(0);
    }
    catch (boost::bad_optional_access const&)
    {
    }
    return boost::report_errors();
}

#endif
