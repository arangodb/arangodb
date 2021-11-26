//
//  Copyright (c) 2012 Artyom Beilis (Tonkikh)
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/nowide/convert.hpp>
#include "test.hpp"
#include "test_sets.hpp"
#include <array>
#include <iostream>
#include <string>

#ifdef __cpp_lib_string_view
#include <string_view>
#define BOOST_NOWIDE_TEST_STD_STRINGVIEW
#endif

#if defined(BOOST_MSVC) && BOOST_MSVC < 1700
#pragma warning(disable : 4428) // universal-character-name encountered in source
#endif

std::wstring widen_buf_ptr(const std::string& s)
{
    wchar_t buf[50];
    TEST(boost::nowide::widen(buf, 50, s.c_str()) == buf);
    return buf;
}

std::string narrow_buf_ptr(const std::wstring& s)
{
    char buf[50];
    TEST(boost::nowide::narrow(buf, 50, s.c_str()) == buf);
    return buf;
}

std::wstring widen_buf_range(const std::string& s)
{
    wchar_t buf[50];
    TEST(boost::nowide::widen(buf, 50, s.c_str(), s.c_str() + s.size()) == buf);
    return buf;
}

std::string narrow_buf_range(const std::wstring& s)
{
    char buf[50];
    TEST(boost::nowide::narrow(buf, 50, s.c_str(), s.c_str() + s.size()) == buf);
    return buf;
}

std::wstring widen_raw_string(const std::string& s)
{
    return boost::nowide::widen(s.c_str());
}

std::string narrow_raw_string(const std::wstring& s)
{
    return boost::nowide::narrow(s.c_str());
}

std::wstring widen_raw_string_and_size(const std::string& s)
{
    // Remove NULL
    const std::string s2 = s + "DummyData";
    return boost::nowide::widen(s2.c_str(), s.size());
}

std::string narrow_raw_string_and_size(const std::wstring& s)
{
    // Remove NULL
    const std::wstring s2 = s + L"DummyData";
    return boost::nowide::narrow(s2.c_str(), s.size());
}

std::wstring widen_convert_buffer(const std::string& s)
{
    std::array<wchar_t, 200> out;
    const auto* result = boost::nowide::utf::convert_buffer(out.data(), out.size(), s.data(), s.data() + s.size());
    TEST(result == out.data());
    return result;
}

std::string narrow_convert_buffer(const std::wstring& s)
{
    std::array<char, 200> out;
    const auto* result = boost::nowide::utf::convert_buffer(out.data(), out.size(), s.data(), s.data() + s.size());
    TEST(result == out.data());
    return result;
}

#ifdef BOOST_NOWIDE_TEST_STD_STRINGVIEW
#define ASSERT_RETURN_TYPE_SV(FUNCTION, CHAR_INPUT, OUTPUT)                                                            \
    static_assert(std::is_same<decltype(FUNCTION(std::declval<std::basic_string_view<CHAR_INPUT>>())), OUTPUT>::value, \
                  "Should be " #OUTPUT);
#else
#define ASSERT_RETURN_TYPE_SV(FUNCTION, CHAR_INPUT, OUTPUT)
#endif
// Check that the functions are callable with various types
#define ASSERT_RETURN_TYPE(FUNCTION, CHAR_INPUT, OUTPUT)                                                          \
    ASSERT_RETURN_TYPE_SV(FUNCTION, CHAR_INPUT, OUTPUT)                                                           \
    static_assert(std::is_same<decltype(FUNCTION(std::declval<const CHAR_INPUT*>())), OUTPUT>::value,             \
                  "Should be " #OUTPUT);                                                                          \
    static_assert(std::is_same<decltype(FUNCTION(std::declval<const CHAR_INPUT*>(), size_t{})), OUTPUT>::value,   \
                  "Should be " #OUTPUT);                                                                          \
    static_assert(std::is_same<decltype(FUNCTION(std::declval<std::basic_string<CHAR_INPUT>>())), OUTPUT>::value, \
                  "Should be " #OUTPUT)

ASSERT_RETURN_TYPE(boost::nowide::widen, char, std::wstring);
#ifdef __cpp_char8_t
ASSERT_RETURN_TYPE(boost::nowide::widen, char8_t, std::wstring);
#endif
ASSERT_RETURN_TYPE(boost::nowide::narrow, wchar_t, std::string);
ASSERT_RETURN_TYPE(boost::nowide::narrow, char16_t, std::string);
ASSERT_RETURN_TYPE(boost::nowide::narrow, char32_t, std::string);

#ifdef BOOST_NOWIDE_TEST_STD_STRINGVIEW
std::wstring widen_string_view(const std::string& s)
{
    return boost::nowide::widen(std::string_view(s));
}

std::string narrow_string_view(const std::wstring& s)
{
    return boost::nowide::narrow(std::wstring_view(s));
}
#endif

// coverity [root_function]
void test_main(int, char**, char**)
{
    std::string hello = "\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d";
    std::wstring whello = L"\u05e9\u05dc\u05d5\u05dd";
    std::wstring whello_3e = L"\u05e9\u05dc\u05d5\ufffd";
    std::wstring whello_3 = L"\u05e9\u05dc\u05d5";

    std::cout << "- boost::nowide::widen" << std::endl;
    {
        const char* b = hello.c_str();
        const char* e = b + hello.size();
        wchar_t buf[6] = {0, 0, 0, 0, 0, 1};
        TEST(boost::nowide::widen(buf, 5, b, e) == buf);
        TEST(buf == whello);
        TEST(buf[5] == 1);
        TEST(boost::nowide::widen(buf, 4, b, e) == 0);
        TEST(boost::nowide::widen(buf, 5, b, e - 1) == buf);
        TEST(buf == whello_3e);
        TEST(boost::nowide::widen(buf, 5, b, e - 2) == buf);
        TEST(buf == whello_3);
        TEST(boost::nowide::widen(buf, 5, b, b) == buf && buf[0] == 0);
        TEST(boost::nowide::widen(buf, 5, b, b + 2) == buf && buf[1] == 0 && buf[0] == whello[0]);

        // Raw literals are also possible
        TEST(boost::nowide::widen("\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d") == whello);
    }
    std::cout << "- boost::nowide::narrow" << std::endl;
    {
        const wchar_t* b = whello.c_str();
        const wchar_t* e = b + whello.size(); //-V594
        char buf[10] = {0};
        buf[9] = 1;
        TEST(boost::nowide::narrow(buf, 9, b, e) == buf);
        TEST(buf == hello);
        TEST(buf[9] == 1);
        TEST(boost::nowide::narrow(buf, 8, b, e) == 0);
        TEST(boost::nowide::narrow(buf, 7, b, e - 1) == buf);
        TEST(buf == hello.substr(0, 6));

        // Raw literals are also possible
        TEST(boost::nowide::narrow(L"\u05e9\u05dc\u05d5\u05dd") == hello);
    }

    std::cout << "- boost::nowide::utf::convert_buffer" << std::endl;
    {
        std::array<wchar_t, 6> buf;
        const char* b = hello.c_str();
        const char* e = b + hello.size();
        using boost::nowide::utf::convert_buffer;
        // Buffer to small (need 5 chars) -> nullptr returned
        for(size_t len = 0; len <= 4; ++len)
            TEST(convert_buffer(buf.data(), len, b, e) == nullptr);
        buf.fill(42);
        TEST(convert_buffer(buf.data(), buf.size(), b, e) == buf.data());
        TEST(buf[4] == 0);      // NULL terminator added
        TEST(buf.back() == 42); // Rest untouched
        TEST(std::wstring(buf.data()) == whello);
    }

    std::cout << "- (output_buffer, buffer_size, input_raw_string)" << std::endl;
    run_all(widen_buf_ptr, narrow_buf_ptr);
    std::cout << "- (output_buffer, buffer_size, input_raw_string, string_len)" << std::endl;
    run_all(widen_buf_range, narrow_buf_range);
    std::cout << "- (input_raw_string)" << std::endl;
    run_all(widen_raw_string, narrow_raw_string);
    std::cout << "- (input_raw_string, size)" << std::endl;
    run_all(widen_raw_string_and_size, narrow_raw_string_and_size);
    std::cout << "- (const std::string&)" << std::endl;
    run_all(boost::nowide::widen, boost::nowide::narrow);
#ifdef BOOST_NOWIDE_TEST_STD_STRINGVIEW
    std::cout << "- (std::string_view)" << std::endl;
    run_all(widen_string_view, narrow_string_view);
#endif
    std::cout << "- (utf::convert_buffer)" << std::endl;
    run_all(widen_convert_buffer, narrow_convert_buffer);
}
