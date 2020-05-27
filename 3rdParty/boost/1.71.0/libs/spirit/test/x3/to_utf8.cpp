/*=============================================================================
    Copyright (c) 2018 Nikita Kniazev

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/core/lightweight_test.hpp>
#include <boost/spirit/home/x3/support/utility/utf8.hpp>

int main()
{
    using boost::spirit::x3::to_utf8;

    // Assume wchar_t is 16-bit on Windows and 32-bit on Unix
#if defined(_WIN32) || defined(__CYGWIN__)
    BOOST_TEST_CSTR_EQ("\xEF\xBF\xA1", to_utf8(L'\uFFE1').c_str());
#else
    BOOST_TEST_CSTR_EQ("\xF0\x9F\xA7\x90", to_utf8(L'\U0001F9D0').c_str());
#endif

    BOOST_TEST_CSTR_EQ("\xF0\x9F\xA7\x90\xF0\x9F\xA7\xA0",
                       to_utf8(L"\U0001F9D0\U0001F9E0").c_str());

    BOOST_TEST_CSTR_EQ("\xF0\x9F\xA7\x90\xF0\x9F\xA7\xA0",
                       to_utf8(std::wstring(L"\U0001F9D0\U0001F9E0")).c_str());

    return boost::report_errors();
}
