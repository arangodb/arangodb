// Boost.Convert test and usage example
// Copyright (c) 2009-2014 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#include "./test.hpp"

#if defined(BOOST_CONVERT_INTEL_SFINAE_BROKEN)
int main(int, char const* []) { return 0; }
#else

#include <boost/convert.hpp>
#include <boost/convert/detail/is_string.hpp>
#include <boost/convert/detail/is_callable.hpp>
#include <boost/detail/lightweight_test.hpp>

//[is_callable_declaration
namespace { namespace local
{
    BOOST_DECLARE_IS_CALLABLE(can_call_funop, operator());
    BOOST_DECLARE_IS_CALLABLE(can_call_func, func);
}}
//]

//[is_callable_classes_tested
namespace { namespace callable
{
    struct  test1 { int  operator()(double, std::string) { return 0; }};
    struct  test2 { void operator()(double, std::string) {}};
    struct  test3 { void operator()(int) {}};
    struct  test4 { std::string operator()(int) const { return std::string(); }};
    struct  test5 { std::string operator()(int, std::string const& =std::string()) const { return std::string(); }};
    struct  test6 { template<typename T> std::string operator()(T) const { return std::string(); }};
    struct  test7 { template<typename T> T operator()(T) const  { return T(); }};

    struct  test11 { int  func(double, std::string) { return 0; }};
    struct  test12 { void func(double, std::string) {}};
    struct  test13 { void func(int) {}};
    struct  test14 { std::string func(int) const { return std::string(); }};
    struct  test15 { std::string func(int, std::string const& =std::string()) const { return std::string(); }};
    struct  test16 { template<typename T> std::string func(T) const { return std::string(); }};
    struct  test17 { template<typename T> T func(T) const { return T(); }};
}}
//]

static
void
test_is_callable()
{
    // C1. Unfortunately, passing 'double' where 'int' is expected returns 'true'.
    //     The same as the following (which successfully compiles):
    //          void fun(int) {}
    //          fun(double(1));

    //[is_callable_usage1
    BOOST_TEST((local::can_call_funop<callable::test1, int (double, std::string)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test1, double (int, std::string)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test1, void (double, std::string)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test1, void (int, std::string)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test1, void (int, char const*)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test1, int (double, int)>::value == false));
    BOOST_TEST((local::can_call_funop<callable::test1, int (double)>::value == false));

    BOOST_TEST((local::can_call_funop<callable::test2, int  (double, std::string)>::value == false));
    BOOST_TEST((local::can_call_funop<callable::test2, void (double, std::string)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test2, void (   int, std::string)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test2, void (   int, char const*)>::value == true));
    //]
    BOOST_TEST((local::can_call_funop<callable::test3,       void (double)>::value == true)); //C1
    BOOST_TEST((local::can_call_funop<callable::test3,       void (int)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test3 const, void (int)>::value == false));

    BOOST_TEST((local::can_call_funop<callable::test4 const, std::string (int)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test4,       std::string (int)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test4 const, void (int)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test4,       void (int)>::value == true));

    BOOST_TEST((local::can_call_funop<callable::test5, std::string (int, std::string)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test5, std::string (int, std::string const&)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test5, void        (int, char const*)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test5,       std::string (int)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test5 const, std::string (int)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test5,       void (int)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test5 const, void (int)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test5,       void (char const*)>::value == false));
    BOOST_TEST((local::can_call_funop<callable::test5 const, void (char const*)>::value == false));

    BOOST_TEST((local::can_call_funop<callable::test6, std::string (int)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test6, std::string (std::string)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test6, void        (int)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test6, void        (std::string)>::value == true));

    BOOST_TEST((local::can_call_funop<callable::test7, std::string (int)>::value == false));
    BOOST_TEST((local::can_call_funop<callable::test7, std::string (std::string)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test7, void        (int)>::value == true));
    BOOST_TEST((local::can_call_funop<callable::test7, void        (std::string)>::value == true));

    //[is_callable_usage2
    BOOST_TEST((local::can_call_func<callable::test11, int (double, std::string)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test11, double (int, std::string)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test11, void (double, std::string)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test11, void (int, std::string)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test11, void (int, char const*)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test11, int (double, int)>::value == false));
    BOOST_TEST((local::can_call_func<callable::test11, int (double)>::value == false));

    BOOST_TEST((local::can_call_func<callable::test12, int  (double, std::string)>::value == false));
    BOOST_TEST((local::can_call_func<callable::test12, void (double, std::string)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test12, void (   int, std::string)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test12, void (   int, char const*)>::value == true));
    //]
    BOOST_TEST((local::can_call_func<callable::test13,       void (double)>::value == true)); //C1
    BOOST_TEST((local::can_call_func<callable::test13,       void (int)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test13 const, void (int)>::value == false));

    BOOST_TEST((local::can_call_func<callable::test14 const, std::string (int)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test14,       std::string (int)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test14 const, void (int)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test14,       void (int)>::value == true));

    BOOST_TEST((local::can_call_func<callable::test15, std::string (int, std::string)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test15, std::string (int, std::string const&)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test15, void        (int, char const*)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test15,       std::string (int)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test15 const, std::string (int)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test15,       void (int)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test15 const, void (int)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test15,       void (char const*)>::value == false));
    BOOST_TEST((local::can_call_func<callable::test15 const, void (char const*)>::value == false));

    BOOST_TEST((local::can_call_func<callable::test16, std::string (int)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test16, std::string (std::string)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test16, void        (int)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test16, void        (std::string)>::value == true));

    BOOST_TEST((local::can_call_func<callable::test17, std::string (int)>::value == false));
    BOOST_TEST((local::can_call_func<callable::test17, std::string (std::string)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test17, void        (int)>::value == true));
    BOOST_TEST((local::can_call_func<callable::test17, void        (std::string)>::value == true));
}

int
main(int, char const* [])
{
    //[is_callable_usage
    //]

    test_is_callable();

    BOOST_TEST(boost::cnv::is_string<direction>::value == false);
    BOOST_TEST(boost::cnv::is_string<std::string>::value == true);
    BOOST_TEST(boost::cnv::is_string<std::wstring>::value == true);
    BOOST_TEST(boost::cnv::is_string<my_string>::value == true);
    BOOST_TEST(boost::cnv::is_string<int>::value == false);

    return boost::report_errors();
}

#endif
