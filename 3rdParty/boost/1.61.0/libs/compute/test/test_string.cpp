//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestString
#include <boost/test/unit_test.hpp>

#include <boost/compute/container/string.hpp>
#include <boost/compute/container/basic_string.hpp>
#include <boost/test/output_test_stream.hpp>

#include "context_setup.hpp"
#include "check_macros.hpp"

using boost::test_tools::output_test_stream;

BOOST_AUTO_TEST_CASE(empty)
{
    boost::compute::string str;
    BOOST_VERIFY(str.empty());
}

BOOST_AUTO_TEST_CASE(swap)
{
    boost::compute::string str1 = "compute";
    boost::compute::string str2 = "boost";
    BOOST_VERIFY(!str2.empty());
    BOOST_VERIFY(!str2.empty()); 
    str1.swap(str2);
    CHECK_STRING_EQUAL(str1, "boost");
    CHECK_STRING_EQUAL(str2, "compute");
    str1.clear();
    str1.swap(str2);
    CHECK_STRING_EQUAL(str1, "compute");
    CHECK_STRING_EQUAL(str2, "");
    str2.swap(str1);
    CHECK_STRING_EQUAL(str1, "");
    CHECK_STRING_EQUAL(str2, "compute");
    str1.swap(str1);
    CHECK_STRING_EQUAL(str1, "");
}

BOOST_AUTO_TEST_CASE(size)
{
    boost::compute::string str = "string";
    BOOST_VERIFY(!str.empty());
    BOOST_CHECK_EQUAL(str.size(), size_t(6));
    BOOST_CHECK_EQUAL(str.length(), size_t(6));
}

BOOST_AUTO_TEST_CASE(find_doctest)
{
//! [string_find]
boost::compute::string str = "boost::compute::string";
int pos = str.find("::");
//! [string_find]
    boost::compute::string pattern = "string";
    BOOST_VERIFY(!str.empty());
    BOOST_CHECK_EQUAL(str.find('o'), 1);
    BOOST_CHECK_NE(str.find('o'), 2);
    BOOST_CHECK_EQUAL(str.find(pattern), 16);
    BOOST_CHECK_EQUAL(pos, 5);
    BOOST_CHECK_EQUAL(str.find("@#$"), size_t(-1));
}

BOOST_AUTO_TEST_CASE(outStream)
{
    output_test_stream output;
    boost::compute::string str = "string";
    output<<str;
    BOOST_CHECK(output.is_equal("string"));
    BOOST_VERIFY(!output.is_equal("!@$%"));
}

BOOST_AUTO_TEST_SUITE_END()
