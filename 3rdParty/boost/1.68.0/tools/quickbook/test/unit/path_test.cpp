/*=============================================================================
    Copyright (c) 2015 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/detail/lightweight_test.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/predef.h>
#include "path.hpp"

void file_path_to_url_tests()
{
    using boost::filesystem::path;
    using quickbook::file_path_to_url;

    BOOST_TEST_EQ(std::string(), file_path_to_url(path()));
    BOOST_TEST_EQ(std::string("."), file_path_to_url(path(".")));
    BOOST_TEST_EQ(std::string("./"), file_path_to_url(path("./")));
    BOOST_TEST_EQ(std::string("a/b"), file_path_to_url(path("a/b")));
    BOOST_TEST_EQ(std::string("a/b/"), file_path_to_url(path("a/b/")));
    BOOST_TEST_EQ(std::string("./a/b"), file_path_to_url(path("./a/./././b")));
    BOOST_TEST_EQ(std::string("../a/b"), file_path_to_url(path("../a/b")));
    BOOST_TEST_EQ(
        std::string("A%20B%2bC%2520"), file_path_to_url(path("A B+C%20")));
    BOOST_TEST_EQ(std::string("file:///"), file_path_to_url(path("/")));
    BOOST_TEST_EQ(std::string("file:///a/b"), file_path_to_url(path("/a/b")));
    BOOST_TEST_EQ(std::string("file:///a/b/"), file_path_to_url(path("/a/b/")));
    BOOST_TEST_EQ(
        std::string("file://hello/a/b"), file_path_to_url(path("//hello/a/b")));

#if BOOST_OS_WINDOWS || BOOST_OS_CYGWIN
    // Should this be file:///c:/x ?
    BOOST_TEST_EQ(
        std::string("file://?/a:/x"), file_path_to_url(path("\\\\?\\a:\\x")));
    BOOST_TEST_EQ(std::string("file:///a"), file_path_to_url(path("\\a")));
    BOOST_TEST_EQ(std::string("file:///c:/"), file_path_to_url(path("c:\\")));
    BOOST_TEST_EQ(
        std::string("file:///c:/foo/bar"),
        file_path_to_url(path("c:\\foo\\bar")));
    BOOST_TEST_EQ(
        std::string("file://localhost/c:/foo/bar"),
        file_path_to_url(path("\\\\localhost\\c:\\foo\\bar")));

    // Really not sure what to do with these examples.
    // Maybe an error?
    BOOST_TEST_EQ(std::string("file:///c:"), file_path_to_url(path("c:")));
    BOOST_TEST_EQ(
        std::string("file:///c:foo/bar"), file_path_to_url(path("c:foo\\bar")));
#endif
}

void dir_path_to_url_tests()
{
    using boost::filesystem::path;
    using quickbook::dir_path_to_url;

    BOOST_TEST_EQ(std::string("./"), dir_path_to_url(path()));
    BOOST_TEST_EQ(std::string("./"), dir_path_to_url(path(".")));
    BOOST_TEST_EQ(std::string("./"), dir_path_to_url(path("./")));
    BOOST_TEST_EQ(std::string("a/b/"), dir_path_to_url(path("a/b")));
    BOOST_TEST_EQ(std::string("a/b/"), dir_path_to_url(path("a/b/")));
    BOOST_TEST_EQ(std::string("./a/b/"), dir_path_to_url(path("./a/./././b")));
    BOOST_TEST_EQ(std::string("../a/b/"), dir_path_to_url(path("../a/b")));
    BOOST_TEST_EQ(
        std::string("A%20B%2bC%2520/"), dir_path_to_url(path("A B+C%20")));
    BOOST_TEST_EQ(std::string("file:///"), dir_path_to_url(path("/")));
    BOOST_TEST_EQ(std::string("file:///a/b/"), dir_path_to_url(path("/a/b")));
    BOOST_TEST_EQ(std::string("file:///a/b/"), dir_path_to_url(path("/a/b/")));
    BOOST_TEST_EQ(
        std::string("file://hello/a/b/"), dir_path_to_url(path("//hello/a/b")));

#if BOOST_OS_WINDOWS || BOOST_OS_CYGWIN
    // Should this be file:///c:/x/ ?
    BOOST_TEST_EQ(
        std::string("file://?/a:/x/"), dir_path_to_url(path("\\\\?\\a:\\x")));
    BOOST_TEST_EQ(std::string("file:///a/"), dir_path_to_url(path("\\a")));
    BOOST_TEST_EQ(std::string("file:///c:/"), dir_path_to_url(path("c:\\")));
    BOOST_TEST_EQ(
        std::string("file:///c:/foo/bar/"),
        dir_path_to_url(path("c:\\foo\\bar")));
    BOOST_TEST_EQ(
        std::string("file://localhost/c:/foo/bar/"),
        dir_path_to_url(path("\\\\localhost\\c:\\foo\\bar")));

    // Really not sure what to do with these examples.
    // Maybe an error?
    BOOST_TEST_EQ(std::string("file:///c:"), dir_path_to_url(path("c:")));
    BOOST_TEST_EQ(
        std::string("file:///c:foo/bar/"), dir_path_to_url(path("c:foo\\bar")));
#endif
}

void path_difference_tests()
{
    using boost::filesystem::current_path;
    using boost::filesystem::path;
    using quickbook::path_difference;

    BOOST_TEST(path(".") == path_difference(path(""), path("")));
    BOOST_TEST(path(".") == path_difference(path("a"), path("a")));
    BOOST_TEST(path(".") == path_difference(path("a/../b"), path("b")));
    BOOST_TEST(path(".") == path_difference(current_path(), current_path()));
    BOOST_TEST(path("..") == path_difference(path("a"), path("")));
    BOOST_TEST(
        path("..") == path_difference(current_path() / "a", current_path()));
    BOOST_TEST(path("a") == path_difference(path(""), path("a")));
    BOOST_TEST(
        path("a") == path_difference(current_path(), current_path() / "a"));
    BOOST_TEST(path("b") == path_difference(path("a"), path("a/b")));
    BOOST_TEST(
        path("b") ==
        path_difference(current_path() / "a", current_path() / "a" / "b"));
    BOOST_TEST(path("../a/b") == path_difference(path("c"), path("a/b")));
    BOOST_TEST(
        path("../a/b") ==
        path_difference(current_path() / "c", current_path() / "a" / "b"));
    BOOST_TEST(path("..") == path_difference(path(""), path("..")));
    BOOST_TEST(
        path("..") ==
        path_difference(current_path(), current_path().parent_path()));
    BOOST_TEST(path("b") == path_difference(path("a/c/.."), path("a/b")));
    BOOST_TEST(path("b") == path_difference(path("b/c/../../a"), path("a/b")));
    BOOST_TEST(
        path("b") ==
        path_difference(path("b/c/../../a"), path("d/f/../../a/b")));
    BOOST_TEST(
        path("../../x/a/b") ==
        path_difference(path("b/c/../../a"), path("d/f/../../../x/a/b")));

    // path_difference to a file, try to include the filename in the result,
    // although not always possible. Maybe nonsense calls should be an error?
    //
    // Commented out cases are wrong because path_difference resolves the paths
    // to the current working directory. In use this doesn't matter as it's
    // always called with the full path, but it'd be nice to get this right.
    // Or maybe just add the pre-condition to path_difference?
    std::cout << path_difference(path(""), path(""), true) << std::endl;
    // BOOST_TEST(path(".") == path_difference(path(""), path(""), true));
    BOOST_TEST(path("../a") == path_difference(path("a"), path("a"), true));
    BOOST_TEST(
        path("../../a") == path_difference(path("a/b"), path("a"), true));
    BOOST_TEST(
        path("../b") == path_difference(path("a/../b"), path("b"), true));
    BOOST_TEST(
        ".." / current_path().filename() ==
        path_difference(current_path(), current_path(), true));
    // BOOST_TEST(path("..") == path_difference(path("a"), path(""), true));
    BOOST_TEST(
        "../.." / current_path().filename() ==
        path_difference(current_path() / "a", current_path(), true));
    BOOST_TEST(path("a") == path_difference(path(""), path("a"), true));
    BOOST_TEST(
        path("a") ==
        path_difference(current_path(), current_path() / "a", true));
    BOOST_TEST(path("b") == path_difference(path("a"), path("a/b"), true));
    BOOST_TEST(
        path("b") ==
        path_difference(
            current_path() / "a", current_path() / "a" / "b", true));
    BOOST_TEST(path("../a/b") == path_difference(path("c"), path("a/b"), true));
    BOOST_TEST(
        path("../a/b") ==
        path_difference(
            current_path() / "c", current_path() / "a" / "b", true));
    // BOOST_TEST(path("..") == path_difference(path(""), path(".."), true));
    BOOST_TEST(
        "../.." / current_path().parent_path().filename() ==
        path_difference(current_path(), current_path().parent_path(), true));
    BOOST_TEST(path("b") == path_difference(path("a/c/.."), path("a/b"), true));
    BOOST_TEST(
        path("b") == path_difference(path("b/c/../../a"), path("a/b"), true));
    BOOST_TEST(
        path("b") ==
        path_difference(path("b/c/../../a"), path("d/f/../../a/b"), true));
    BOOST_TEST(
        path("../../x/a/b") ==
        path_difference(path("b/c/../../a"), path("d/f/../../../x/a/b"), true));
}

int main()
{
    file_path_to_url_tests();
    dir_path_to_url_tests();
    path_difference_tests();
    return boost::report_errors();
}
