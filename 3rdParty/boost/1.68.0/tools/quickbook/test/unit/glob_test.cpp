/*=============================================================================
    Copyright (c) 2013 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/detail/lightweight_test.hpp>
#include "glob.hpp"

void glob_tests()
{
    BOOST_TEST(quickbook::glob("", ""));

    BOOST_TEST(!quickbook::glob("*", ""));
    BOOST_TEST(quickbook::glob("*", "a"));
    BOOST_TEST(quickbook::glob("*b", "b"));
    BOOST_TEST(quickbook::glob("*b", "ab"));
    BOOST_TEST(quickbook::glob("*b", "bab"));
    BOOST_TEST(quickbook::glob("*b*", "b"));
    BOOST_TEST(quickbook::glob("*b*", "ab"));
    BOOST_TEST(quickbook::glob("*b*", "bc"));
    BOOST_TEST(quickbook::glob("*b*", "abc"));
    BOOST_TEST(!quickbook::glob("*b*", ""));
    BOOST_TEST(!quickbook::glob("*b*", "a"));
    BOOST_TEST(!quickbook::glob("*b*", "ac"));

    BOOST_TEST(quickbook::glob("hello.txt", "hello.txt"));
    BOOST_TEST(!quickbook::glob("world.txt", "helloworld.txt"));
    BOOST_TEST(quickbook::glob("*world.txt", "helloworld.txt"));
    BOOST_TEST(!quickbook::glob("world.txt*", "helloworld.txt"));
    BOOST_TEST(!quickbook::glob("hello", "helloworld.txt"));
    BOOST_TEST(!quickbook::glob("*hello", "helloworld.txt"));
    BOOST_TEST(quickbook::glob("hello*", "helloworld.txt"));
    BOOST_TEST(quickbook::glob("*world*", "helloworld.txt"));

    BOOST_TEST(quickbook::glob("?", "a"));
    BOOST_TEST(!quickbook::glob("?", ""));
    BOOST_TEST(!quickbook::glob("?", "ab"));
    BOOST_TEST(quickbook::glob("a?", "ab"));
    BOOST_TEST(quickbook::glob("?b", "ab"));
    BOOST_TEST(quickbook::glob("?bc", "abc"));
    BOOST_TEST(quickbook::glob("a?c", "abc"));
    BOOST_TEST(quickbook::glob("ab?", "abc"));
    BOOST_TEST(!quickbook::glob("?bc", "aac"));
    BOOST_TEST(!quickbook::glob("a?c", "bbc"));
    BOOST_TEST(!quickbook::glob("ab?", "abcd"));

    BOOST_TEST(quickbook::glob("[a]", "a"));
    BOOST_TEST(!quickbook::glob("[^a]", "a"));
    BOOST_TEST(!quickbook::glob("[b]", "a"));
    BOOST_TEST(quickbook::glob("[^b]", "a"));
    BOOST_TEST(quickbook::glob("[a-z]", "a"));
    BOOST_TEST(!quickbook::glob("[^a-z]", "a"));
    BOOST_TEST(!quickbook::glob("[b-z]", "a"));
    BOOST_TEST(quickbook::glob("[^b-z]", "a"));
    BOOST_TEST(quickbook::glob("[-a]", "a"));
    BOOST_TEST(quickbook::glob("[-a]", "-"));
    BOOST_TEST(!quickbook::glob("[-a]", "b"));
    BOOST_TEST(!quickbook::glob("[^-a]", "a"));
    BOOST_TEST(!quickbook::glob("[^-a]", "-"));
    BOOST_TEST(quickbook::glob("[^-a]", "b"));
    BOOST_TEST(quickbook::glob("[a-]", "a"));
    BOOST_TEST(quickbook::glob("[a-]", "-"));
    BOOST_TEST(!quickbook::glob("[a-]", "b"));
    BOOST_TEST(!quickbook::glob("[^a-]", "a"));
    BOOST_TEST(!quickbook::glob("[^a-]", "-"));
    BOOST_TEST(quickbook::glob("[^a-]", "b"));
    BOOST_TEST(quickbook::glob("[a-ce-f]", "a"));
    BOOST_TEST(!quickbook::glob("[a-ce-f]", "d"));
    BOOST_TEST(quickbook::glob("[a-ce-f]", "f"));
    BOOST_TEST(!quickbook::glob("[a-ce-f]", "g"));
    BOOST_TEST(!quickbook::glob("[^a-ce-f]", "a"));
    BOOST_TEST(quickbook::glob("[^a-ce-f]", "d"));
    BOOST_TEST(!quickbook::glob("[^a-ce-f]", "f"));
    BOOST_TEST(quickbook::glob("[^a-ce-f]", "g"));
    BOOST_TEST(!quickbook::glob("[b]", "a"));
    BOOST_TEST(quickbook::glob("[a]bc", "abc"));
    BOOST_TEST(quickbook::glob("a[b]c", "abc"));
    BOOST_TEST(quickbook::glob("ab[c]", "abc"));
    BOOST_TEST(quickbook::glob("a[a-c]c", "abc"));
    BOOST_TEST(quickbook::glob("*[b]*", "abc"));
    BOOST_TEST(quickbook::glob("[\\]]", "]"));
    BOOST_TEST(!quickbook::glob("[^\\]]", "]"));

    BOOST_TEST(quickbook::glob("b*ana", "banana"));
    BOOST_TEST(quickbook::glob("1234*1234*1234", "123412341234"));
    BOOST_TEST(!quickbook::glob("1234*1234*1234", "1234123341234"));
    BOOST_TEST(quickbook::glob("1234*1234*1234", "123412312312341231231234"));
    BOOST_TEST(!quickbook::glob("1234*1234*1234", "12341231231234123123123"));
}

void invalid_glob_tests()
{
    // Note that glob only throws an exception when the pattern matches up to
    // the point where the error occurs.
    BOOST_TEST_THROWS(quickbook::glob("[", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("[^", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("[xyz", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("[xyz\\", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("[x\\y", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("[a-", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("[a-z", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("[a-\\", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("[a-\\a", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("xyx[", "xyxa"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("]", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("abc]", "abcd"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("]def", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("[]", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("[[]", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("[]]", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("**", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("[/]", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("[\\/]", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("[ -/]", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("[ -\\/]", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("\\", "a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::glob("\\\\", "a"), quickbook::glob_error);
}

void check_glob_tests()
{
    BOOST_TEST(!quickbook::check_glob(""));
    BOOST_TEST(!quickbook::check_glob("file"));
    BOOST_TEST(!quickbook::check_glob("file\\[\\]"));
    BOOST_TEST(quickbook::check_glob("[x]"));
    BOOST_TEST(quickbook::check_glob("abc[x]"));
    BOOST_TEST(quickbook::check_glob("[x]abd"));
    BOOST_TEST_THROWS(quickbook::check_glob("["), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("[^"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("[xyz"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("[xyz\\"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("[x\\y"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("[a-"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("[a-z"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("[a-\\"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("[a-\\a"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("xyx["), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("]"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("abc]"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("]def"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("[]"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("[[]"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("[]]"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("**"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("[/]"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("[\\/]"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("[ -/]"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("[ -\\/]"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("\\"), quickbook::glob_error);
    BOOST_TEST_THROWS(quickbook::check_glob("\\\\"), quickbook::glob_error);
}

int main()
{
    glob_tests();
    invalid_glob_tests();
    check_glob_tests();

    return boost::report_errors();
}
