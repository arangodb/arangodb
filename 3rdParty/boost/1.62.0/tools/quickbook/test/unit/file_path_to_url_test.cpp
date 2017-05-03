/*=============================================================================
    Copyright (c) 2015 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/detail/lightweight_test.hpp>
#include "include_paths.hpp"

int main() {
    using boost::filesystem::path;
    using quickbook::file_path_to_url;

    BOOST_TEST_EQ("a/b", file_path_to_url(path("a/b")));
    BOOST_TEST_EQ("../a/b", file_path_to_url(path("../a/b")));
    BOOST_TEST_EQ("A%20B%2bC%2520", file_path_to_url(path("A B+C%20")));

    // TODO: Windows specific tests.

    return boost::report_errors();
}
