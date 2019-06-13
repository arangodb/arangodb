/*=============================================================================
    Copyright (c) 2009 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "native_text.hpp"
#include <iostream>
#include <boost/program_options.hpp>
#include "utils.hpp"

#if QUICKBOOK_WIDE_PATHS || QUICKBOOK_WIDE_STREAMS
#include <windows.h>
#include <boost/scoped_array.hpp>
#endif

namespace quickbook
{
    namespace detail
    {
// This is used for converting paths to UTF-8 on cygin.
// Might be better not to use a windows
#if QUICKBOOK_WIDE_PATHS || QUICKBOOK_WIDE_STREAMS
        std::string to_utf8(std::wstring const& x)
        {
            int buffer_count =
                WideCharToMultiByte(CP_UTF8, 0, x.c_str(), -1, 0, 0, 0, 0);

            if (!buffer_count)
                throw conversion_error(
                    "Error converting wide string to utf-8.");

            boost::scoped_array<char> buffer(new char[buffer_count]);

            if (!WideCharToMultiByte(
                    CP_UTF8, 0, x.c_str(), -1, buffer.get(), buffer_count, 0,
                    0))
                throw conversion_error(
                    "Error converting wide string to utf-8.");

            return std::string(buffer.get());
        }

        std::wstring from_utf8(quickbook::string_view text)
        {
            std::string x(text.begin(), text.end());
            int buffer_count =
                MultiByteToWideChar(CP_UTF8, 0, x.c_str(), -1, 0, 0);

            if (!buffer_count)
                throw conversion_error(
                    "Error converting utf-8 to wide string.");

            boost::scoped_array<wchar_t> buffer(new wchar_t[buffer_count]);

            if (!MultiByteToWideChar(
                    CP_UTF8, 0, x.c_str(), -1, buffer.get(), buffer_count))
                throw conversion_error(
                    "Error converting utf-8 to wide string.");

            return std::wstring(buffer.get());
        }
#endif
    }
}
