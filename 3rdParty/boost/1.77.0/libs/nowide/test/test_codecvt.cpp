//
//  Copyright (c) 2015 Artyom Beilis (Tonkikh)
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/nowide/utf8_codecvt.hpp>

#include <boost/nowide/convert.hpp>
#include "test.hpp"
#include "test_sets.hpp"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <locale>
#include <vector>

static const char* utf8_name =
  "\xf0\x9d\x92\x9e-\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82-\xE3\x82\x84\xE3\x81\x82.txt";
static const std::wstring wide_name_str = boost::nowide::widen(utf8_name);
static const wchar_t* wide_name = wide_name_str.c_str();

using cvt_type = std::codecvt<wchar_t, char, std::mbstate_t>;

void test_codecvt_in_n_m(const cvt_type& cvt, size_t n, size_t m)
{
    const wchar_t* wptr = wide_name;
    size_t wlen = std::wcslen(wide_name);
    size_t u8len = std::strlen(utf8_name);
    const char* from = utf8_name;
    const char* from_end = from;
    const char* real_end = utf8_name + u8len;
    const char* from_next = from;
    std::mbstate_t mb{};
    while(from_next < real_end)
    {
        if(from == from_end)
        {
            from_end = from + n;
            if(from_end > real_end)
                from_end = real_end;
        }

        wchar_t buf[128];
        wchar_t* to = buf;
        wchar_t* to_end = to + m;
        wchar_t* to_next = to;

        std::mbstate_t mb2 = mb;
        std::codecvt_base::result r = cvt.in(mb, from, from_end, from_next, to, to_end, to_next);

        int count = cvt.length(mb2, from, from_end, to_end - to);
        TEST(std::memcmp(&mb, &mb2, sizeof(mb)) == 0);
        TEST(count == from_next - from);

        if(r == cvt_type::partial)
        {
            from_end += n;
            if(from_end > real_end)
                from_end = real_end;
        } else
            TEST(r == cvt_type::ok);
        while(to != to_next)
        {
            TEST(*wptr == *to);
            wptr++;
            to++;
        }
        to = to_next;
        from = from_next;
    }
    TEST(wptr == wide_name + wlen);
    TEST(from == real_end);
}

void test_codecvt_out_n_m(const cvt_type& cvt, size_t n, size_t m)
{
    const char* nptr = utf8_name;
    size_t wlen = std::wcslen(wide_name);
    size_t u8len = std::strlen(utf8_name);

    std::mbstate_t mb{};

    const wchar_t* from_next = wide_name;
    const wchar_t* real_from_end = wide_name + wlen;

    char buf[256];
    char* to = buf;
    char* to_next = to;
    char* to_end = to + n;
    char* real_to_end = buf + sizeof(buf);

    while(from_next < real_from_end)
    {
        const wchar_t* from = from_next;
        const wchar_t* from_end = from + m;
        if(from_end > real_from_end)
            from_end = real_from_end;
        if(to_end == to)
        {
            to_end = to + n;
        }

        std::codecvt_base::result r = cvt.out(mb, from, from_end, from_next, to, to_end, to_next);
        if(r == cvt_type::partial)
        {
            // If those are equal, then "partial" probably means: Need more input
            // Otherwise "Need more output"
            if(from_next != from_end)
            {
                TEST(to_end - to_next < cvt.max_length());
                to_end += n;
                if(to_end > real_to_end)
                    to_end = real_to_end;
            }
        } else
        {
            TEST(r == cvt_type::ok);
        }

        while(to != to_next)
        {
            TEST(*nptr == *to);
            nptr++;
            to++;
        }
        from = from_next;
    }
    TEST(nptr == utf8_name + u8len);
    TEST(from_next == real_from_end);
    TEST(cvt.unshift(mb, to, to + n, to_next) == cvt_type::ok);
    TEST(to_next == to);
}

void test_codecvt_conv()
{
    std::cout << "Conversions " << std::endl;
    std::locale l(std::locale::classic(), new boost::nowide::utf8_codecvt<wchar_t>());

    const cvt_type& cvt = std::use_facet<cvt_type>(l);
    const size_t utf8_len = std::strlen(utf8_name);
    const size_t wide_len = std::wcslen(wide_name);

    for(size_t i = 1; i <= utf8_len + 1; i++)
    {
        for(size_t j = 1; j <= wide_len + 1; j++)
        {
            try
            {
                test_codecvt_in_n_m(cvt, i, j);
                test_codecvt_out_n_m(cvt, i, j);
            } catch(...)
            {
                std::cerr << "Wlen=" << j << " Nlen=" << i << std::endl;
                throw;
            }
        }
    }
}

void test_codecvt_err()
{
    std::cout << "Errors " << std::endl;
    std::locale l(std::locale::classic(), new boost::nowide::utf8_codecvt<wchar_t>());

    const cvt_type& cvt = std::use_facet<cvt_type>(l);

    std::cout << "- UTF-8" << std::endl;
    {
        {
            wchar_t buf[4];
            wchar_t* const to = buf;
            wchar_t* const to_end = buf + 4;
            const char* err_utf = "1\xFF\xFF\xd7\xa9";
            std::mbstate_t mb{};
            const char* from = err_utf;
            const char* from_end = from + std::strlen(from);
            const char* from_next = from;
            wchar_t* to_next = to;
            TEST(cvt.in(mb, from, from_end, from_next, to, to_end, to_next) == cvt_type::ok);
            TEST(from_next == from + 5);
            TEST(to_next == to + 4);
            TEST(std::wstring(to, to_end) == boost::nowide::widen(err_utf));
        }
        {
            wchar_t buf[4];
            wchar_t* const to = buf;
            wchar_t* const to_end = buf + 4;
            const char* err_utf = "1\xd7"; // 1 valid, 1 incomplete UTF-8 char
            std::mbstate_t mb{};
            const char* from = err_utf;
            const char* from_end = from + std::strlen(from);
            const char* from_next = from;
            wchar_t* to_next = to;
            TEST(cvt.in(mb, from, from_end, from_next, to, to_end, to_next) == cvt_type::partial);
            TEST(from_next == from + 1);
            TEST(to_next == to + 1);
            TEST(std::wstring(to, to_next) == std::wstring(L"1"));
        }
        {
            char buf[4] = {};
            char* const to = buf;
            char* const to_end = buf + 4;
            char* to_next = to;
            const wchar_t* err_utf = L"\xD800"; // Trailing UTF-16 surrogate
            std::mbstate_t mb{};
            const wchar_t* from = err_utf;
            const wchar_t* from_end = from + 1;
            const wchar_t* from_next = from;
            cvt_type::result res = cvt.out(mb, from, from_end, from_next, to, to_end, to_next);
#ifdef BOOST_MSVC
#pragma warning(disable : 4127) // Constant expression detected
#endif
            if(sizeof(wchar_t) == 2)
            {
                TEST(res == cvt_type::partial);
                TEST(from_next == from_end);
                TEST(to_next == to);
                TEST(buf[0] == 0);
            } else
            {
                TEST(res == cvt_type::ok);
                TEST(from_next == from_end);
                TEST(to_next == to + 3);
                // surrogate is invalid
                TEST(std::string(to, to_next) == boost::nowide::narrow(wreplacement_str));
            }
        }
    }

    std::cout << "- UTF-16/32" << std::endl;
    {
        char buf[32];
        char* to = buf;
        char* to_end = buf + 32;
        char* to_next = to;
        wchar_t err_buf[3] = {'1', 0xDC9E, 0}; // second surrogate not works both for UTF-16 and 32
        const wchar_t* err_utf = err_buf;
        {
            std::mbstate_t mb{};
            const wchar_t* from = err_utf;
            const wchar_t* from_end = from + std::wcslen(from);
            const wchar_t* from_next = from;
            TEST(cvt.out(mb, from, from_end, from_next, to, to_end, to_next) == cvt_type::ok);
            TEST(from_next == from + 2);
            TEST(to_next == to + 4);
            TEST(std::string(to, to_next) == "1" + boost::nowide::narrow(wreplacement_str));
        }
    }
}

std::wstring codecvt_to_wide(const std::string& s)
{
    std::locale l(std::locale::classic(), new boost::nowide::utf8_codecvt<wchar_t>());

    const cvt_type& cvt = std::use_facet<cvt_type>(l);

    std::mbstate_t mb{};
    const char* const from = s.c_str();
    const char* const from_end = from + s.size();
    const char* from_next = from;

    std::vector<wchar_t> buf(s.size() + 2); // +1 for possible incomplete char, +1 for NULL
    wchar_t* const to = &buf[0];
    wchar_t* const to_end = to + buf.size();
    wchar_t* to_next = to;

    cvt_type::result res = cvt.in(mb, from, from_end, from_next, to, to_end, to_next);
    if(res == cvt_type::partial)
    {
        TEST(to_next < to_end);
        *(to_next++) = BOOST_NOWIDE_REPLACEMENT_CHARACTER;
    } else
        TEST(res == cvt_type::ok);

    return std::wstring(to, to_next);
}

std::string codecvt_to_narrow(const std::wstring& s)
{
    std::locale l(std::locale::classic(), new boost::nowide::utf8_codecvt<wchar_t>());

    const cvt_type& cvt = std::use_facet<cvt_type>(l);

    std::mbstate_t mb{};
    const wchar_t* const from = s.c_str();
    const wchar_t* const from_end = from + s.size();
    const wchar_t* from_next = from;

    std::vector<char> buf((s.size() + 1) * 4 + 1); // +1 for possible incomplete char, +1 for NULL
    char* const to = &buf[0];
    char* const to_end = to + buf.size();
    char* to_next = to;

    cvt_type::result res = cvt.out(mb, from, from_end, from_next, to, to_end, to_next);
    if(res == cvt_type::partial)
    {
        TEST(to_next < to_end);
        return std::string(to, to_next) + boost::nowide::narrow(wreplacement_str);
    } else
        TEST(res == cvt_type::ok);

    return std::string(to, to_next);
}

void test_codecvt_subst()
{
    std::cout << "Substitutions " << std::endl;
    run_all(codecvt_to_wide, codecvt_to_narrow);
}

// coverity [root_function]
void test_main(int, char**, char**)
{
    test_codecvt_conv();
    test_codecvt_err();
    test_codecvt_subst();
}
