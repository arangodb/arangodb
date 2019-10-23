// Copyright Antony Polukhin, 2016-2019.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_STACKTRACE_TEST_IMPL_LIB 1
#include "test_impl.hpp"

using namespace boost::stacktrace;

BOOST_ST_API BOOST_NOINLINE std::pair<stacktrace, stacktrace> function_from_library(int i, foo1_t foo1) {
    if (i) {
        return foo1(--i);
    } else {
        return foo1(i);
    }
}


namespace very_very_very_very_very_very_long_namespace {
namespace very_very_very_very_very_very_long_namespace {
namespace very_very_very_very_very_very_long_namespace {
namespace very_very_very_very_very_very_long_namespace {
namespace very_very_very_very_very_very_long_namespace {
namespace very_very_very_very_very_very_long_namespace {
namespace very_very_very_very_very_very_long_namespace {
namespace very_very_very_very_very_very_long_namespace {
namespace very_very_very_very_very_very_long_namespace {
namespace very_very_very_very_very_very_long_namespace {
    BOOST_ST_API BOOST_NOINLINE stacktrace get_backtrace_from_nested_namespaces() {
        return stacktrace();
    }
}}}}}}}}}}

BOOST_ST_API BOOST_NOINLINE stacktrace return_from_nested_namespaces() {
    using very_very_very_very_very_very_long_namespace::very_very_very_very_very_very_long_namespace::very_very_very_very_very_very_long_namespace
        ::very_very_very_very_very_very_long_namespace::very_very_very_very_very_very_long_namespace::very_very_very_very_very_very_long_namespace
        ::very_very_very_very_very_very_long_namespace::very_very_very_very_very_very_long_namespace::very_very_very_very_very_very_long_namespace
        ::very_very_very_very_very_very_long_namespace::get_backtrace_from_nested_namespaces;

    return get_backtrace_from_nested_namespaces();
}

BOOST_ST_API BOOST_NOINLINE boost::stacktrace::stacktrace make_some_stacktrace1_impl(int d = 0) {
    boost::stacktrace::stacktrace result(0, 4);
    if (result.size() < 4) {
        if (d > 4) throw std::runtime_error("Stack is not growing in test OR stacktrace fails to work in `bar1` function.");
        return make_some_stacktrace1_impl(d + 1);
    }
    return result;
}

BOOST_ST_API BOOST_NOINLINE boost::stacktrace::stacktrace make_some_stacktrace2_impl(int d = 0) {
    boost::stacktrace::stacktrace result(0, 4);
    if (result.size() < 4) {
        if (d > 4) throw std::runtime_error("Stack is not growing in test OR stacktrace fails to work in `bar2` function.");
        return make_some_stacktrace2_impl(d + 1);
    }
    return result;
}

BOOST_ST_API BOOST_NOINLINE boost::stacktrace::stacktrace make_some_stacktrace1() {
    boost::stacktrace::stacktrace result = make_some_stacktrace1_impl();
    return result;
}

BOOST_ST_API BOOST_NOINLINE boost::stacktrace::stacktrace make_some_stacktrace2() {
    boost::stacktrace::stacktrace result = make_some_stacktrace2_impl();
    return result;
}

