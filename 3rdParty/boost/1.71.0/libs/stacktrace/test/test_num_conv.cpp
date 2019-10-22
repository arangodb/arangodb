// Copyright Antony Polukhin, 2016-2019.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/stacktrace/detail/to_dec_array.hpp>
#include <boost/stacktrace/detail/to_hex_array.hpp>
#include <boost/stacktrace/detail/try_dec_convert.hpp>

#include <boost/core/lightweight_test.hpp>
#include <string>
#include <iostream>


void test_to_hex_array() {
    const void* ptr = 0;
    BOOST_TEST(std::string(boost::stacktrace::detail::to_hex_array(ptr).data()).find("0x0") != std::string::npos);

    ptr = reinterpret_cast<const void*>(0x10);
    BOOST_TEST(std::string(boost::stacktrace::detail::to_hex_array(ptr).data()).find("10") != std::string::npos);

    ptr = reinterpret_cast<void*>(0x19);
    BOOST_TEST(std::string(boost::stacktrace::detail::to_hex_array(ptr).data()).find("19") != std::string::npos);

    ptr = reinterpret_cast<void*>(0x999999);
    BOOST_TEST(std::string(boost::stacktrace::detail::to_hex_array(ptr).data()).find("999999") != std::string::npos);
}

void test_to_dec_array() {
    BOOST_TEST_EQ(std::string(boost::stacktrace::detail::to_dec_array(0).data()), std::string("0"));
    BOOST_TEST_EQ(std::string(boost::stacktrace::detail::to_dec_array(10).data()), std::string("10"));
    BOOST_TEST_EQ(std::string(boost::stacktrace::detail::to_dec_array(19).data()), std::string("19"));
    BOOST_TEST_EQ(std::string(boost::stacktrace::detail::to_dec_array(999999).data()), std::string("999999"));
}

void test_try_dec_convert() {
    std::size_t res = 0;

    BOOST_TEST(boost::stacktrace::detail::try_dec_convert("0", res));
    BOOST_TEST(res == 0);

    BOOST_TEST(boost::stacktrace::detail::try_dec_convert("+0", res));
    BOOST_TEST(res == 0);

    BOOST_TEST(boost::stacktrace::detail::try_dec_convert("10", res));
    BOOST_TEST(res == 10);

    BOOST_TEST(boost::stacktrace::detail::try_dec_convert("19", res));
    BOOST_TEST(res == 19);

    BOOST_TEST(boost::stacktrace::detail::try_dec_convert("+19", res));
    BOOST_TEST(res == 19);

    BOOST_TEST(boost::stacktrace::detail::try_dec_convert("9999", res));
    BOOST_TEST(res == 9999);

    BOOST_TEST(!boost::stacktrace::detail::try_dec_convert("q", res));
    BOOST_TEST(!boost::stacktrace::detail::try_dec_convert("0z", res));
    BOOST_TEST(!boost::stacktrace::detail::try_dec_convert("0u", res));
    BOOST_TEST(!boost::stacktrace::detail::try_dec_convert("+0u", res));
}


int main() {
    test_to_hex_array();
    test_to_dec_array();
    test_try_dec_convert();

    return boost::report_errors();
}
