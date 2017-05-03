//-----------------------------------------------------------------------------
// boost-libs variant/test/recursive_variant_test.cpp source file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2003 Eric Friedman, Itay Maman
// Copyright (c) 2013 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


// This file is used in two test cases:
//
// 1) recursive_variant_test.cpp that tests recursive usage of variant
//
// 2) variant_noexcept_test that tests Boost.Variant ability to compile 
// and work with disabled exceptions

#ifdef BOOST_NO_EXCEPTIONS
// `boost/test/minimal.hpp` cannot work with exceptions disabled,
// so we need the following workarounds for that case:
namespace boost {
    int exit_success = 0;
}

int test_main(int , char* []);

int main( int argc, char* argv[] )
{
    return test_main(argc, argv);
}

#include <stdlib.h>
#define BOOST_CHECK(exp) if (!(exp)) exit(EXIT_FAILURE)

#else // BOOST_NO_EXCEPTIONS
#   include "boost/test/minimal.hpp"
#endif // BOOST_NO_EXCEPTIONS


#include "boost/variant.hpp"
#include "boost/mpl/vector.hpp"
#include "boost/mpl/copy.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#if !defined(BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_HDR_TUPLE)
#include <tuple>
#endif // !defined(BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_HDR_TUPLE)

struct printer
    : boost::static_visitor<std::string>
{
    template <BOOST_VARIANT_ENUM_PARAMS(typename T)>
    std::string operator()(
            const boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)> &var) const
    {
        return boost::apply_visitor( printer(), var );
    }

    template <typename T>
    std::string operator()(const std::vector<T>& vec) const
    {
        std::ostringstream ost;

        ost << "( ";

        typename std::vector<T>::const_iterator it = vec.begin();
        for (; it != vec.end(); ++it)
            ost << printer()( *it );

        ost << ") ";

        return ost.str();
    }

#if !defined(BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_HDR_TUPLE)
    template <int...> struct indices {};
    template <typename... Ts, int... Is>
    std::string operator()(const std::tuple<Ts...>& tup, indices<Is...>) const
    {
        std::ostringstream ost;
        ost << "( ";
        int a[] = {0, (ost << printer()( std::get<Is>(tup) ), 0)... };
        (void)a;
        ost << ") ";
        return ost.str();
    }

    template <int N, int... Is>
        struct make_indices : make_indices<N-1, N-1, Is...> {};
    template <int... Is>
        struct make_indices<0, Is...> : indices<Is...> {};
    template <typename... Ts>
    std::string operator()(const std::tuple<Ts...>& tup) const
    {
        return printer()(tup, make_indices<sizeof...(Ts)>());
    }
#endif // !defined(BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_HDR_TUPLE)

    template <typename T>
    std::string operator()(const T& operand) const
    {
        std::ostringstream ost;
        ost << operand << ' ';
        return ost.str();
    }
};

void test_recursive_variant()
{
    typedef boost::make_recursive_variant<
          int
        , std::vector<boost::recursive_variant_>
        >::type var1_t;

    std::vector<var1_t> vec1;
    vec1.push_back(3);
    vec1.push_back(5);
    vec1.push_back(vec1);
    vec1.push_back(7);

    var1_t var1(vec1);
    std::string result1( printer()(var1) );

    std::cout << "result1: " << result1 << '\n';
    BOOST_CHECK(result1 == "( 3 5 ( 3 5 ) 7 ) ");

    std::vector<var1_t> vec1_copy = vec1;
    vec1_copy.erase(vec1_copy.begin() + 2);
    vec1_copy.insert(vec1_copy.begin() + 2, vec1_copy);
    var1 = vec1_copy;
    result1 = printer()(var1);
    std::cout << "result1+: " << result1 << '\n';
    BOOST_CHECK(result1 == "( 3 5 ( 3 5 7 ) 7 ) ");

    // Uses move construction on compilers with rvalue references support
    result1 = printer()(
        var1_t(
            std::vector<var1_t>(vec1_copy)
        )
    );
    std::cout << "result1++: " << result1 << '\n';
    BOOST_CHECK(result1 == "( 3 5 ( 3 5 7 ) 7 ) ");


    var1_t vec1_another_copy(vec1_copy);
    vec1_copy[2].swap(vec1_another_copy);
    result1 = printer()(
        var1_t(vec1_copy)
    );
    std::cout << "result1+++1: " << result1 << '\n';
    BOOST_CHECK(result1 == "( 3 5 ( 3 5 ( 3 5 7 ) 7 ) 7 ) ");

    result1 = printer()(vec1_another_copy);
    std::cout << "result1++2: " << result1 << '\n';
    BOOST_CHECK(result1 == "( 3 5 7 ) ");

    vec1_copy[2].swap(vec1_copy[2]);
    result1 = printer()(
        var1_t(vec1_copy)
    );
    std::cout << "result1.2: " << result1 << '\n';
    BOOST_CHECK(result1 == "( 3 5 ( 3 5 ( 3 5 7 ) 7 ) 7 ) ");

    typedef boost::make_recursive_variant<
          boost::variant<int, double>
        , std::vector<boost::recursive_variant_>
        >::type var2_t;

    std::vector<var2_t> vec2;
    vec2.push_back(boost::variant<int, double>(3));
    vec2.push_back(boost::variant<int, double>(3.5));
    vec2.push_back(vec2);
    vec2.push_back(boost::variant<int, double>(7));

    var2_t var2(vec2);
    std::string result2( printer()(var2) );

    std::cout << "result2: " << result2 << '\n';
    BOOST_CHECK(result2 == "( 3 3.5 ( 3 3.5 ) 7 ) ");
    
    typedef boost::make_recursive_variant<
          int
        , std::vector<
              boost::variant<
                    double
                  , std::vector<boost::recursive_variant_>
                  >
              >
        >::type var3_t;

    typedef boost::variant<double, std::vector<var3_t> > var4_t;

    std::vector<var3_t> vec3;
    vec3.push_back(3);
    vec3.push_back(5);
    std::vector<var4_t> vec4;
    vec4.push_back(3.5);
    vec4.push_back(vec3);
    vec3.push_back(vec4);
    vec3.push_back(7);

    var4_t var4(vec3);
    std::string result3( printer()(var4) );

    std::cout << "result2: " << result3 << '\n';
    BOOST_CHECK(result3 == "( 3 5 ( 3.5 ( 3 5 ) ) 7 ) ");

    typedef boost::make_recursive_variant<
          double,
          std::vector<var1_t>
        >::type var5_t;

    std::vector<var5_t> vec5;
    vec5.push_back(3.5);
    vec5.push_back(vec1);
    vec5.push_back(17.25);

    std::string result5( printer()(vec5) );

    std::cout << "result5: " << result5 << '\n';
    BOOST_CHECK(result5 == "( 3.5 ( 3 5 ( 3 5 ) 7 ) 17.25 ) ");

    typedef boost::make_recursive_variant<
          int,
          std::map<int, boost::recursive_variant_>
        >::type var6_t;
    var6_t var6;

#if !defined(BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_HDR_TUPLE)
    typedef boost::make_recursive_variant<
          int,
          std::tuple<int, boost::recursive_variant_>
        >::type var7_t;
    var7_t var7 = 0;    // !!! Do not replace with `var7_t var7{0}` or `var7_t var7(0)` !!!
    var7 = std::tuple<int, var7_t>(1, var7);
    var7 = std::tuple<int, var7_t>(2, var7);

    std::string result7( printer()(var7) );

    std::cout << "result7: " << result7 << '\n';
    BOOST_CHECK(result7 == "( 2 ( 1 0 ) ) ");
#endif // !defined(BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_HDR_TUPLE)
}

void test_recursive_variant_over()
{
    typedef boost::make_recursive_variant_over<
          boost::mpl::vector<
              int
            , std::vector<boost::recursive_variant_>
          >
        >::type var1_t;

    std::vector<var1_t> vec1;
    vec1.push_back(3);
    vec1.push_back(5);
    vec1.push_back(vec1);
    vec1.push_back(7);

    var1_t var1(vec1);
    std::string result1( printer()(var1) );

    std::cout << "result1: " << result1 << '\n';
    BOOST_CHECK(result1 == "( 3 5 ( 3 5 ) 7 ) ");

    std::vector<var1_t> vec1_copy = vec1;
    vec1_copy.erase(vec1_copy.begin() + 2);
    vec1_copy.insert(vec1_copy.begin() + 2, vec1_copy);
    var1 = vec1_copy;
    result1 = printer()(var1);
    std::cout << "result1+: " << result1 << '\n';
    BOOST_CHECK(result1 == "( 3 5 ( 3 5 7 ) 7 ) ");

    // Uses move construction on compilers with rvalue references support
    result1 = printer()(
        var1_t(
            std::vector<var1_t>(vec1_copy)
        )
    );
    std::cout << "result1++: " << result1 << '\n';
    BOOST_CHECK(result1 == "( 3 5 ( 3 5 7 ) 7 ) ");


    var1_t vec1_another_copy(vec1_copy);
    vec1_copy[2].swap(vec1_another_copy);
    result1 = printer()(
        var1_t(vec1_copy)
    );
    std::cout << "result1+++1: " << result1 << '\n';
    BOOST_CHECK(result1 == "( 3 5 ( 3 5 ( 3 5 7 ) 7 ) 7 ) ");

    result1 = printer()(vec1_another_copy);
    std::cout << "result1++2: " << result1 << '\n';
    BOOST_CHECK(result1 == "( 3 5 7 ) ");

    typedef boost::make_recursive_variant_over<
          boost::mpl::vector<
              boost::make_variant_over<boost::mpl::vector<int, double> >::type
            , std::vector<boost::recursive_variant_>
            >
        >::type var2_t;

    std::vector<var2_t> vec2;
    vec2.push_back(boost::variant<int, double>(3));
    vec2.push_back(boost::variant<int, double>(3.5));
    vec2.push_back(vec2);
    vec2.push_back(boost::variant<int, double>(7));

    var2_t var2(vec2);
    std::string result2( printer()(var2) );

    std::cout << "result2: " << result2 << '\n';
    BOOST_CHECK(result2 == "( 3 3.5 ( 3 3.5 ) 7 ) ");
    
    typedef boost::make_recursive_variant_over<
          boost::mpl::vector<
              int
            , std::vector<
                  boost::make_variant_over<
                      boost::mpl::vector<
                          double
                        , std::vector<boost::recursive_variant_>
                        >
                    >::type
                >
            >
        >::type var3_t;

    typedef boost::make_variant_over<
          boost::mpl::copy<
              boost::mpl::vector<
                  double
                , std::vector<var3_t>
                >
            >::type
        >::type var4_t;

    std::vector<var3_t> vec3;
    vec3.push_back(3);
    vec3.push_back(5);
    std::vector<var4_t> vec4;
    vec4.push_back(3.5);
    vec4.push_back(vec3);
    vec3.push_back(vec4);
    vec3.push_back(7);

    var4_t var3(vec3);
    std::string result3( printer()(var3) );

    std::cout << "result2: " << result3 << '\n';
    BOOST_CHECK(result3 == "( 3 5 ( 3.5 ( 3 5 ) ) 7 ) ");
    
    typedef boost::make_recursive_variant_over<
          boost::mpl::vector<
              double
            , std::vector<var1_t>
            >
        >::type var5_t;

    std::vector<var5_t> vec5;
    vec5.push_back(3.5);
    vec5.push_back(vec1);
    vec5.push_back(17.25);

    std::string result5( printer()(vec5) );

    std::cout << "result5: " << result5 << '\n';
    BOOST_CHECK(result5 == "( 3.5 ( 3 5 ( 3 5 ) 7 ) 17.25 ) ");
}

int test_main(int , char* [])
{
    test_recursive_variant();
    test_recursive_variant_over();

    return boost::exit_success;
}
