// Copyright (c) 2009-2014 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#ifdef BOOST_MSVC
#  pragma warning(disable : 4127)  // conditional expression is constant.
#  pragma warning(disable : 4189)  // local variable is initialized but not referenced.
#endif

#include <boost/convert.hpp>
#include <boost/convert/stream.hpp>
#include <boost/convert/lexical_cast.hpp>

using std::string;
using boost::convert;
using boost::lexical_cast;
using boost::optional;

static void process_failure() {}
static void log(...) {}
static int fallback_function() { return -1; }

//[getting_serious_default_converter
struct boost::cnv::by_default : public boost::cnv::cstream {};
//]
static
void
example1()
{
    boost::cnv::cstream  cnv;
    std::string const    str = "123";
    std::string const   str1 = "123";
    std::string const   str2 = "123";
    std::string const   str3 = "123";
    int const fallback_value = -1;

    {
        //[getting_serious_example1
        int i2 = convert<int>("not an int", cnv).value_or(-1); // after the call i2==-1

        if (i2 == -1) process_failure();
        //]
    }
    {
        //[getting_serious_example2
        try
        {
            int i1 = lexical_cast<int>(str);         // Throws if the conversion fails.
            int i2 = convert<int>(str, cnv).value(); // Throws if the conversion fails.
        }
        catch (...)
        {
            process_failure();
        }
        //]
    }
    {
        //[getting_serious_example3
        optional<int> r1 = convert<int>(str1, cnv); // Does not throw on conversion failure.
        optional<int> r2 = convert<int>(str2, cnv); // Does not throw on conversion failure.
        // ...
        try // Delayed processing of potential exceptions.
        {
            int i1 = r1.value(); // Will throw if conversion failed.
            int i2 = r2.value(); // Will throw if conversion failed.
        }
        catch (boost::bad_optional_access const&)
        {
            // Handle failed conversion.
        }

        // Exceptions are avoided altogether.
        int i1 = r1 ? r1.value() : fallback_value;
        int i2 = r2.value_or(fallback_value);
        int i3 = convert<int>(str3, cnv).value_or(fallback_value);
        int i4 = convert<int>(str3, cnv).value_or_eval(fallback_function);
        //]
    }
}

//[getting_serious_example5
struct fallback_func
{
    int operator()() const { log("Failed to convert"); return 42; }
};
//]

static
void
example4()
{
    boost::cnv::cstream  cnv;
    std::string const    str = "123";
    int const fallback_value = -1;
    //[getting_serious_example4
    boost::optional<int> res = boost::convert<int>(str, cnv);

    if (!res) log("str conversion failed!");

    int i1 = res.value_or(fallback_value);

    // ...proceed
    //]
    //[getting_serious_example6
    // Fallback function is called when failed
    int i2 = convert<int>(str, cnv).value_or_eval(fallback_func());
    int i3 = convert<int>(str, cnv, fallback_func()); // Same as above. Alternative API.
    //]
}

static
void
example7()
{
    boost::cnv::cstream  cnv;
    std::string const    str = "123";
    int const fallback_value = -1;
    //[getting_serious_example7
    // Error-processing behavior are specified unambiguously and uniformly.
    // a) i1: Returns the provided fallback value;
    // b) i2: Calls the provided failure-processing function;
    // c) i3: Throws an exception.

    int i1 = convert<int>(str, cnv, fallback_value);
    int i2 = convert<int>(str, cnv, fallback_func());

    try
    {
        // Throwing behavior specified explicitly rather than implied.
        int i3 = convert<int>(str, cnv, boost::throw_on_failure);
    }
    catch (boost::bad_optional_access const&)
    {
      // Handle failed conversion.
    }
    //]
    //[getting_serious_example8
    int m1 = convert<int>(str, cnv).value_or(fallback_value);
    int m2 = convert<int>(str, cnv).value_or_eval(fallback_func());
    int m3 = convert<int>(str, cnv).value();
    //]
    //[getting_serious_example9
    int n1 = convert<int>(str).value_or(fallback_value);
    int n2 = convert<int>(str).value_or_eval(fallback_func());
    int n3 = convert<int>(str).value();
    //]
}

int
main(int, char const* [])
{
    example1();
    example4();
    example7();
}
