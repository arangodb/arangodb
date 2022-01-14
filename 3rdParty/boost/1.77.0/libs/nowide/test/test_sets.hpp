//
//  Copyright (c) 2012 Artyom Beilis (Tonkikh)
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef BOOST_NOWIDE_TEST_SETS_HPP_INCLUDED
#define BOOST_NOWIDE_TEST_SETS_HPP_INCLUDED

#include <boost/nowide/config.hpp>
#include <iostream>
#include <string>

struct utf8_to_wide
{
    const char* utf8;
    const wchar_t* wide;
};

struct wide_to_utf8
{
    const wchar_t* wide;
    const char* utf8;
};

#if defined(BOOST_MSVC) && BOOST_MSVC < 1700
#pragma warning(disable : 4428) // universal-character-name encountered in source
#endif

const std::wstring wreplacement_str(1, wchar_t(BOOST_NOWIDE_REPLACEMENT_CHARACTER));

// clang-format off
const utf8_to_wide roundtrip_tests[] = {
    {"", L""},
    // Ascii
    {"a", L"a"},
    // 2 Octet
    {"\xc3\xb1", L"\u00F1"},
    // 3 Octet
    {"\xe2\x82\xa1", L"\u20A1"},
    // 4 Octet
    {"\xf0\x90\x8c\xbc", L"\U0001033C"},
    // Last valid codepoint
    {"\xf4\x8f\xbf\xbf", L"\U0010FFFF"},
    // Misc
    {"\xf0\x9d\x92\x9e-\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82-\xE3\x82\x84\xE3\x81\x82.txt",
    L"\U0001D49E-\u043F\u0440\u0438\u0432\u0435\u0442-\u3084\u3042.txt"},
    {"\xd7\xa9-\xd0\xbc-\xce\xbd.txt",
    L"\u05e9-\u043c-\u03bd.txt"},
    {"\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d",
    L"\u05e9\u05dc\u05d5\u05dd"},
};

const utf8_to_wide invalid_utf8_tests[] = {
    // 2 Octet
    {"\xc3\x28", L"\ufffd"},
    {"\xa0\xa1", L"\ufffd\ufffd"},
    // 3 Octet
    {"\xe2\x28\xa1", L"\ufffd\ufffd"},
    {"\xe2\x82\x28", L"\ufffd"},
    // 4 Octet
    {"\xf0\x28\x8c\xbc", L"\ufffd\ufffd\ufffd"},
    {"\xf0\x90\x28\xbc", L"\ufffd\ufffd"},
    {"\xf0\x90\x8c\x28", L"\ufffd"},
    // 5 and 6 byte possible but invalid UTF
    {"\xf8\xa1\xa1\xa1\xa1", L"\ufffd\ufffd\ufffd\ufffd\ufffd"},
    {"\xfc\xa1\xa1\xa1\xa1\xa1", L"\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd"},
    // First invalid codepoint
    {"\xf4\x90\x80\x80", L"\ufffd\ufffd\ufffd\ufffd"},
    // Overlong ascii (0x2F),
    {"\xc0\xaf", L"\ufffd\ufffd"},
    {"\xe0\x80\xaf", L"\ufffd\ufffd\ufffd"},
    {"\xf0\x80\x80\xaf", L"\ufffd\ufffd\ufffd\ufffd"},
    {"\xf8\x80\x80\x80\xaf", L"\ufffd\ufffd\ufffd\ufffd\ufffd"},
    {"\xfc\x80\x80\x80\x80\xaf", L"\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd"},
    // Misc
    {"\xFF\xFF", L"\ufffd\ufffd"},
    {"\xd7\xa9\xFF", L"\u05e9\ufffd"},
    {"\xd7", L"\ufffd"},
    {"\xFF\xd7\xa9", L"\ufffd\u05e9"},
    {"\xFF\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82", L"\uFFFD\u043F\u0440\u0438\u0432\u0435\u0442"},
    {"\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82\xFF", L"\u043F\u0440\u0438\u0432\u0435\u0442\uFFFD"},
    {"\xE3\x82\xFF\xE3\x81\x82", L"\ufffd\u3042"},
    {"\xE3\xFF\x84\xE3\x81\x82", L"\ufffd\ufffd\u3042"},
};

const wide_to_utf8 invalid_wide_tests[] = {
  {L"\xDC01\x05e9", "\xEF\xBF\xBD\xd7\xa9"},
  {L"\x05e9\xD800", "\xd7\xa9\xEF\xBF\xBD"},
  {L"\xDC00\x20\u043F\u0440\u0438\u0432\u0435\u0442-\u3084\u3042",
   "\xEF\xBF\xBD \xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82-\xE3\x82\x84\xE3\x81\x82"},
  {L"\u3084\u3042\xDC00\x20\u043F\u0440\u0438\u0432\u0435\u0442-\u3084\u3042",
   "\xE3\x82\x84\xE3\x81\x82\xEF\xBF\xBD \xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82-\xE3\x82\x84\xE3\x81\x82"},
};


const wide_to_utf8 invalid_utf16_tests[] = {
  {L"\xD800\x20\u043F\u0440\u0438\u0432\u0435\u0442-\u3084\u3042",
   "\xEF\xBF\xBD\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82-\xE3\x82\x84\xE3\x81\x82"},
  {L"\u3084\u3042\xD800\x20\u043F\u0440\u0438\u0432\u0435\u0442-\u3084\u3042",
   "\xE3\x82\x84\xE3\x81\x82\xEF\xBF\xBD\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82-\xE3\x82\x84\xE3\x81\x82"},
};

const wide_to_utf8 invalid_utf32_tests[] = {
  {L"\xD800\x20\u043F\u0440\u0438\u0432\u0435\u0442-\u3084\u3042",
   "\xEF\xBF\xBD \xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82-\xE3\x82\x84\xE3\x81\x82"},
  {L"\u3084\u3042\xD800\x20\u043F\u0440\u0438\u0432\u0435\u0442-\u3084\u3042",
   "\xE3\x82\x84\xE3\x81\x82\xEF\xBF\xBD \xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82-\xE3\x82\x84\xE3\x81\x82"},
};

// clang-format on

#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable : 4127) // Constant expression detected
#endif

template<typename T, size_t N>
constexpr size_t array_size(const T (&)[N])
{
    return N;
}

void run_all(std::wstring (*to_wide)(const std::string&), std::string (*to_narrow)(const std::wstring&))
{
    for(size_t i = 0; i < array_size(roundtrip_tests); i++)
    {
        std::cout << "  Roundtrip  " << i << std::endl;
        TEST(roundtrip_tests[i].utf8 == to_narrow(roundtrip_tests[i].wide));
        TEST(to_wide(roundtrip_tests[i].utf8) == roundtrip_tests[i].wide);
    }

    for(size_t i = 0; i < array_size(invalid_utf8_tests); i++)
    {
        std::cout << "  Invalid UTF8  " << i << std::endl;
        const auto f3 = to_wide(invalid_utf8_tests[i].utf8);
        TEST(to_wide(invalid_utf8_tests[i].utf8) == invalid_utf8_tests[i].wide);
    }

    for(size_t i = 0; i < array_size(invalid_wide_tests); i++)
    {
        std::cout << "  Invalid Wide  " << i << std::endl;
        TEST(to_narrow(invalid_wide_tests[i].wide) == invalid_wide_tests[i].utf8);
    }

    size_t total = 0;
    const wide_to_utf8* ptr = 0;
    if(sizeof(wchar_t) == 2)
    {
        ptr = invalid_utf16_tests;
        total = array_size(invalid_utf16_tests);
    } else
    {
        ptr = invalid_utf32_tests;
        total = array_size(invalid_utf32_tests);
    }
    for(size_t i = 0; i < total; i++)
    {
        std::cout << "  Invalid UTF16/32  " << i << std::endl;
        TEST(to_narrow(ptr[i].wide) == ptr[i].utf8);
    }
}

#endif

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
