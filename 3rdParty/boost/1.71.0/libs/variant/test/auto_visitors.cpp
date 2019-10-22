//-----------------------------------------------------------------------------
// boost-libs variant/test/auto_visitors.cpp source file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2014-2019 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "boost/config.hpp"

#include "boost/core/lightweight_test.hpp"
#include "boost/variant.hpp"
#include "boost/variant/multivisitors.hpp"
#include "boost/lexical_cast.hpp"

#include <boost/noncopyable.hpp>
#include <boost/core/ignore_unused.hpp>

namespace has_result_type_tests {
    template <class T>
    struct wrap {
        typedef T result_type;
    };

    struct s1 : wrap<int> {};
    struct s2 : wrap<int&> {};
    struct s3 : wrap<const int&> {};
    struct s4 {};
    struct s5 : wrap<int*> {};
    struct s6 : wrap<int**> {};
    struct s7 : wrap<const int*> {};
    struct s8 : wrap<boost::noncopyable> {};
    struct s9 : wrap<boost::noncopyable&> {};
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    struct s10 : wrap<boost::noncopyable&&> {};
#endif
    struct s11 : wrap<const boost::noncopyable&> {};
    struct s12 : wrap<const boost::noncopyable*> {};
    struct s13 : wrap<boost::noncopyable*> {};
    struct s14 { typedef int result_type; };
    struct s15 { typedef int& result_type; };
    struct s16 { typedef const int& result_type; };
}


void test_has_result_type_triat() {
    using namespace has_result_type_tests;
    using boost::detail::variant::has_result_type;

    BOOST_TEST(has_result_type<s1>::value);
    BOOST_TEST(has_result_type<s2>::value);
    BOOST_TEST(has_result_type<s3>::value);
    BOOST_TEST(!has_result_type<s4>::value);
    BOOST_TEST(has_result_type<s5>::value);
    BOOST_TEST(has_result_type<s6>::value);
    BOOST_TEST(has_result_type<s7>::value);
    BOOST_TEST(has_result_type<s8>::value);
    BOOST_TEST(has_result_type<s9>::value);
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    BOOST_TEST(has_result_type<s10>::value);
#endif
    BOOST_TEST(has_result_type<s11>::value);
    BOOST_TEST(has_result_type<s12>::value);
    BOOST_TEST(has_result_type<s13>::value);
    BOOST_TEST(has_result_type<s14>::value);
    BOOST_TEST(has_result_type<s15>::value);
    BOOST_TEST(has_result_type<s16>::value);
}

struct lex_streamer_explicit: boost::static_visitor<std::string> {
    template <class T>
    const char* operator()(const T& ) {
        return "10";
    }

    template <class T1, class T2>
    const char* operator()(const T1& , const T2& ) {
        return "100";
    }
};


void run_explicit()
{
    typedef boost::variant<int, std::string, double> variant_type;
    variant_type v2("10"), v1("100");

    lex_streamer_explicit visitor_ref;

    // Must return instance of std::string
    BOOST_TEST(boost::apply_visitor(visitor_ref, v2).c_str() == std::string("10"));
    BOOST_TEST(boost::apply_visitor(visitor_ref, v2, v1).c_str() == std::string("100"));
}


// Most part of tests from this file require decltype(auto)

#ifdef BOOST_NO_CXX14_DECLTYPE_AUTO

void run()
{
    BOOST_TEST(true);
}

void run2()
{
    BOOST_TEST(true);
}


void run3()
{
    BOOST_TEST(true);
}

#else

#include <iostream>

struct lex_streamer {
    template <class T>
    std::string operator()(const T& val) const {
        return boost::lexical_cast<std::string>(val);
    }
};

struct lex_streamer_void {
    template <class T>
    void operator()(const T& val) const {
        std::cout << val << std::endl;
    }


    template <class T1, class T2>
    void operator()(const T1& val, const T2& val2) const {
        std::cout << val << '+' << val2 << std::endl;
    }


    template <class T1, class T2, class T3>
    void operator()(const T1& val, const T2& val2, const T3& val3) const {
        std::cout << val << '+' << val2 << '+' << val3 << std::endl;
    }
};


struct lex_streamer2 {
    std::string res;

    template <class T>
    const char* operator()(const T& /*val*/) const {
        return "fail";
    }

    template <class T1, class T2>
    const char* operator()(const T1& /*v1*/, const T2& /*v2*/) const {
        return "fail2";
    }


    template <class T1, class T2, class T3>
    const char* operator()(const T1& /*v1*/, const T2& /*v2*/, const T3& /*v3*/) const {
        return "fail3";
    }

    template <class T>
    std::string& operator()(const T& val) {
        res = boost::lexical_cast<std::string>(val);
        return res;
    }


    template <class T1, class T2>
    std::string& operator()(const T1& v1, const T2& v2) {
        res = boost::lexical_cast<std::string>(v1) + "+" + boost::lexical_cast<std::string>(v2);
        return res;
    }


    template <class T1, class T2, class T3>
    std::string& operator()(const T1& v1, const T2& v2, const T3& v3) {
        res = boost::lexical_cast<std::string>(v1) + "+" + boost::lexical_cast<std::string>(v2) 
            + "+" + boost::lexical_cast<std::string>(v3);
        return res;
    }
};

#ifndef BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES
#   define BOOST_TEST_IF_HAS_VARIADIC(x) BOOST_TEST(x)
#else
#   define BOOST_TEST_IF_HAS_VARIADIC(x) /**/
#endif

void run()
{
    typedef boost::variant<int, std::string, double> variant_type;
    variant_type v1(1), v2("10"), v3(100.0);
    lex_streamer lex_streamer_visitor;

    BOOST_TEST(boost::apply_visitor(lex_streamer(), v1) == "1");
    BOOST_TEST_IF_HAS_VARIADIC(boost::apply_visitor(lex_streamer_visitor)(v1) == "1");
    BOOST_TEST(boost::apply_visitor(lex_streamer(), v2) == "10");
    BOOST_TEST_IF_HAS_VARIADIC(boost::apply_visitor(lex_streamer_visitor)(v2) == "10");

    #ifndef BOOST_NO_CXX14_GENERIC_LAMBDAS
        BOOST_TEST(boost::apply_visitor([](auto v) { return boost::lexical_cast<std::string>(v); }, v1) == "1");
        BOOST_TEST(boost::apply_visitor([](auto v) { return boost::lexical_cast<std::string>(v); }, v2) == "10");

        // Retun type must be the same in all instances, so this code does not compile
        //boost::variant<int, short, unsigned> v_diff_types(1);
        //BOOST_TEST(boost::apply_visitor([](auto v) { return v; }, v_diff_types) == 1);

        boost::apply_visitor([](auto v) { std::cout << v << std::endl; }, v1);
        boost::apply_visitor([](auto v) { std::cout << v << std::endl; }, v2);
    #endif

    lex_streamer2 visitor_ref;
    BOOST_TEST(boost::apply_visitor(visitor_ref, v1) == "1");
    BOOST_TEST(boost::apply_visitor(visitor_ref, v2) == "10");
#ifndef BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES
    std::string& ref_to_string = boost::apply_visitor(visitor_ref, v1);
    BOOST_TEST(ref_to_string == "1");
#endif
    lex_streamer_void lex_streamer_void_visitor;
    boost::apply_visitor(lex_streamer_void(), v1);
    boost::apply_visitor(lex_streamer_void(), v2);
#ifndef BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES
    boost::apply_visitor(lex_streamer_void_visitor)(v2);
#endif

    boost::ignore_unused(lex_streamer_visitor, visitor_ref, lex_streamer_void_visitor);
}


struct lex_combine {
    template <class T1, class T2>
    std::string operator()(const T1& v1, const T2& v2) const {
        return boost::lexical_cast<std::string>(v1) + "+" + boost::lexical_cast<std::string>(v2);
    }


    template <class T1, class T2, class T3>
    std::string operator()(const T1& v1, const T2& v2, const T3& v3) const {
        return boost::lexical_cast<std::string>(v1) + "+" 
                + boost::lexical_cast<std::string>(v2) + '+'
                + boost::lexical_cast<std::string>(v3);
    }
};

void run2()
{
    typedef boost::variant<int, std::string, double> variant_type;
    variant_type v1(1), v2("10"), v3(100.0);
    lex_combine lex_combine_visitor;

    BOOST_TEST(boost::apply_visitor(lex_combine(), v1, v2) == "1+10");
    BOOST_TEST(boost::apply_visitor(lex_combine(), v2, v1) == "10+1");
    BOOST_TEST_IF_HAS_VARIADIC(boost::apply_visitor(lex_combine_visitor)(v2, v1) == "10+1");


    #ifndef BOOST_NO_CXX14_GENERIC_LAMBDAS
        BOOST_TEST(
            boost::apply_visitor(
                [](auto v1, auto v2) {
                    return boost::lexical_cast<std::string>(v1) + "+"
                        + boost::lexical_cast<std::string>(v2);
                }
                , v1
                , v2
            ) == "1+10"
        );
        BOOST_TEST(
            boost::apply_visitor(
                [](auto v1, auto v2) {
                    return boost::lexical_cast<std::string>(v1) + "+"
                        + boost::lexical_cast<std::string>(v2);
                }
                , v2
                , v1
            ) == "10+1"
        );

        boost::apply_visitor([](auto v1, auto v2) { std::cout << v1 << '+' << v2 << std::endl; }, v1, v2);
        boost::apply_visitor([](auto v1, auto v2) { std::cout << v1 << '+' << v2 << std::endl; }, v2, v1);
    #endif


    lex_streamer2 visitor_ref;
    BOOST_TEST(boost::apply_visitor(visitor_ref, v1, v2) == "1+10");
    BOOST_TEST(boost::apply_visitor(visitor_ref, v2, v1) == "10+1");
#ifndef BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES
    std::string& ref_to_string = boost::apply_visitor(visitor_ref)(v1, v2);
    BOOST_TEST(ref_to_string == "1+10");
#endif

    boost::apply_visitor(lex_streamer_void(), v1, v2);
    boost::apply_visitor(lex_streamer_void(), v2, v1);

    boost::ignore_unused(lex_combine_visitor, visitor_ref);
}

#undef BOOST_TEST_IF_HAS_VARIADIC

void run3()
{
#if !defined(BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_HDR_TUPLE)
    typedef boost::variant<int, std::string, double> variant_type;
    variant_type v1(1), v2("10"), v3(100);
    lex_combine lex_combine_visitor;

    BOOST_TEST(boost::apply_visitor(lex_combine(), v1, v2, v3) == "1+10+100");
    BOOST_TEST(boost::apply_visitor(lex_combine(), v2, v1, v3) == "10+1+100");
    BOOST_TEST(boost::apply_visitor(lex_combine_visitor)(v2, v1, v3) == "10+1+100");


    #ifndef BOOST_NO_CXX14_GENERIC_LAMBDAS
        BOOST_TEST(
            boost::apply_visitor(
                [](auto v1, auto v2, auto v3) {
                    return boost::lexical_cast<std::string>(v1) + "+"
                        + boost::lexical_cast<std::string>(v2) + "+"
                        + boost::lexical_cast<std::string>(v3);
                }
                , v1
                , v2
                , v3
            ) == "1+10+100"
        );
        BOOST_TEST(
            boost::apply_visitor(
                [](auto v1, auto v2, auto v3) {
                    return boost::lexical_cast<std::string>(v1) + "+"
                        + boost::lexical_cast<std::string>(v2) + "+"
                        + boost::lexical_cast<std::string>(v3);
                }
                , v3
                , v1
                , v3
            ) == "100+1+100"
        );

        boost::apply_visitor(
            [](auto v1, auto v2, auto v3) { std::cout << v1 << '+' << v2 << '+' << v3 << std::endl; }, 
            v1, v2, v3
        );
        boost::apply_visitor(
            [](auto v1, auto v2, auto v3) { std::cout << v1 << '+' << v2 << '+' << v3 << std::endl; },
            v2, v1, v3
        );
    #endif


    lex_streamer2 visitor_ref;
    BOOST_TEST(boost::apply_visitor(visitor_ref, v1, v2) == "1+10");
    BOOST_TEST(boost::apply_visitor(visitor_ref)(v2, v1) == "10+1");
    std::string& ref_to_string = boost::apply_visitor(visitor_ref, v1, v2);
    BOOST_TEST(ref_to_string == "1+10");

    lex_streamer_void lex_streamer_void_visitor;
    boost::apply_visitor(lex_streamer_void(), v1, v2, v1);
    boost::apply_visitor(lex_streamer_void(), v2, v1, v1);
    boost::apply_visitor(lex_streamer_void_visitor)(v2, v1, v1);
#endif // !defined(BOOST_VARIANT_DO_NOT_USE_VARIADIC_TEMPLATES) && !defined(BOOST_NO_CXX11_HDR_TUPLE)
}
#endif


int main()
{
    run_explicit();
    run();
    run2();
    run3();
    test_has_result_type_triat();

    return boost::report_errors();
}
