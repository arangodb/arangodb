// Copyright Antony Polukhin, 2016-2017.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <boost/stacktrace.hpp>
#include <boost/core/lightweight_test.hpp>
#include <stdexcept>

#include <boost/functional/hash.hpp>

using boost::stacktrace::stacktrace;
using boost::stacktrace::frame;

#ifdef BOOST_STACKTRACE_DYN_LINK
#   define BOOST_ST_API BOOST_SYMBOL_IMPORT
#else
#   define BOOST_ST_API
#endif

typedef std::pair<stacktrace, stacktrace> (*foo1_t)(int i);
BOOST_ST_API std::pair<stacktrace, stacktrace> foo2(int i, foo1_t foo1);
BOOST_ST_API stacktrace return_from_nested_namespaces();


BOOST_NOINLINE std::pair<stacktrace, stacktrace> foo1(int i) {
    if (i) {
        return foo2(i - 1, foo1);
    }

    std::pair<stacktrace, stacktrace> ret;
    try {
        throw std::logic_error("test");
    } catch (const std::logic_error& /*e*/) {
        ret.second = stacktrace();
        return ret;
    }
}

void test_deeply_nested_namespaces() {
    BOOST_TEST(return_from_nested_namespaces().size() == 0);
    BOOST_TEST(return_from_nested_namespaces().empty());
    BOOST_TEST(!return_from_nested_namespaces());
}

void test_nested() {
    std::pair<stacktrace, stacktrace> res = foo2(15, foo1);

    BOOST_TEST(!res.first);
    BOOST_TEST(res.first.empty());
    BOOST_TEST(res.first.size() == 0);

    BOOST_TEST(res.second <= res.first);
    BOOST_TEST(res.second >= res.first);
    BOOST_TEST(res.second == res.first);
    BOOST_TEST(res.second == res.first);
    BOOST_TEST(!(res.second > res.first));
}

void test_empty_frame() {
    boost::stacktrace::frame empty_frame;
    BOOST_TEST(!empty_frame);
    BOOST_TEST(empty_frame.source_file() == "");
    BOOST_TEST(empty_frame.name() == "");
    BOOST_TEST(empty_frame.source_line() == 0);

    boost::stacktrace::frame f(0);
    BOOST_TEST(f.name() == "");
    BOOST_TEST(f.source_file() == "");
    BOOST_TEST(f.source_line() == 0);
}

int main() {
    test_deeply_nested_namespaces();
    test_nested();
    test_empty_frame();

    return boost::report_errors();
}
