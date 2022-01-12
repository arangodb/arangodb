// Copyright Antony Polukhin, 2016-2021.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_impl.hpp"

#include <boost/stacktrace.hpp>
#include <boost/core/lightweight_test.hpp>
#include <stdexcept>


#include <boost/functional/hash.hpp>

using boost::stacktrace::stacktrace;
using boost::stacktrace::frame;

void test_deeply_nested_namespaces() {
    BOOST_TEST(return_from_nested_namespaces().size() == 0);
    BOOST_TEST(return_from_nested_namespaces().empty());
    BOOST_TEST(!return_from_nested_namespaces());
}

void test_nested() {
    std::pair<stacktrace, stacktrace> res = function_from_library(15, function_from_main_translation_unit);

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
