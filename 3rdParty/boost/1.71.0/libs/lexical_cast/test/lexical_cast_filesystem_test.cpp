//  Unit test for boost::lexical_cast.
//
//  See http://www.boost.org for most recent version, including documentation.
//
//  Copyright Antony Polukhin, 2013-2019.
//
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt).
//
// Test lexical_cast usage with long filesystem::path. Bug 7704.

#include <boost/config.hpp>

#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem/path.hpp>

using namespace boost;

void test_filesystem();

unit_test::test_suite *init_unit_test_suite(int, char *[])
{
    unit_test::test_suite *suite =
        BOOST_TEST_SUITE("lexical_cast+filesystem unit test");
    suite->add(BOOST_TEST_CASE(&test_filesystem));

    return suite;
}

void test_filesystem()
{
    boost::filesystem::path p;
    std::string s1 = "aaaaaaaaaaaaaaaaaaaaaaa";
    p = boost::lexical_cast<boost::filesystem::path>(s1);
    BOOST_CHECK(!p.empty());
    BOOST_CHECK_EQUAL(p, s1);
    p.clear();

    const char ab[] = "aaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    p = boost::lexical_cast<boost::filesystem::path>(ab);
    BOOST_CHECK(!p.empty());
    BOOST_CHECK_EQUAL(p, ab);
    
    // Tests for
    // https://github.com/boostorg/lexical_cast/issues/25

    const char quoted_path[] = "\"/home/my user\"";
    p = boost::lexical_cast<boost::filesystem::path>(quoted_path);
    BOOST_CHECK(!p.empty());
    const char unquoted_path[] = "/home/my user";
    BOOST_CHECK_EQUAL(p, boost::filesystem::path(unquoted_path));

    // Converting back to std::string gives the initial string
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(p), quoted_path);
    
    try {
        // Without quotes the path will have only `/home/my` in it.
        // `user` remains in the stream, so an exception must be thrown.
        p = boost::lexical_cast<boost::filesystem::path>(unquoted_path);
        BOOST_CHECK(false);
    } catch (const boost::bad_lexical_cast& ) {
        // Exception is expected
    }
}

