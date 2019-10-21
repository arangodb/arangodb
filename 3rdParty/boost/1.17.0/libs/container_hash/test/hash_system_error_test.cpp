
// Copyright 2018 Daniel James
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "./config.hpp"

#ifndef BOOST_HASH_TEST_STD_INCLUDES
#  include <boost/container_hash/hash.hpp>
#endif
#include <boost/config.hpp>
#include <boost/core/lightweight_test.hpp>

#if !defined(BOOST_NO_CXX11_HDR_SYSTEM_ERROR)

#include <system_error>

void test_error_code()
{
    std::error_code err1a = std::make_error_code(std::errc::argument_list_too_long);
    std::error_code err1b = std::make_error_code(std::errc::argument_list_too_long);
    std::error_code err2 = std::make_error_code(std::errc::bad_file_descriptor);

    boost::hash<std::error_code> hasher;

    BOOST_TEST(hasher(err1a) == hasher(err1a));
    BOOST_TEST(hasher(err1a) == hasher(err1b));
    BOOST_TEST(hasher(err1a) != hasher(err2));
}

void test_error_condition()
{
    std::error_condition err1a = std::make_error_condition(std::errc::directory_not_empty);
    std::error_condition err1b = std::make_error_condition(std::errc::directory_not_empty);
    std::error_condition err2 = std::make_error_condition(std::errc::filename_too_long);

    boost::hash<std::error_condition> hasher;

    BOOST_TEST(hasher(err1a) == hasher(err1a));
    BOOST_TEST(hasher(err1a) == hasher(err1b));
    BOOST_TEST(hasher(err1a) != hasher(err2));
}

#endif

int main()
{
#if !defined(BOOST_NO_CXX11_HDR_SYSTEM_ERROR)
    test_error_code();
    test_error_condition();
#else
    BOOST_LIGHTWEIGHT_TEST_OSTREAM << "<system_error> not available." << std::endl;
#endif
    return boost::report_errors();
}
