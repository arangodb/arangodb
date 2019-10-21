/*=============================================================================
    Copyright (c) 2009 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// For handling native strings and streams.

#if !defined(BOOST_QUICKBOOK_DETAIL_NATIVE_TEXT_HPP)
#define BOOST_QUICKBOOK_DETAIL_NATIVE_TEXT_HPP

#include <stdexcept>
#include <string>
#include <boost/config.hpp>
#include "fwd.hpp"
#include "string_view.hpp"

#if defined(__cygwin__) || defined(__CYGWIN__)
#define QUICKBOOK_CYGWIN_PATHS 1
#elif defined(_WIN32)
#define QUICKBOOK_WIDE_PATHS 1
// Wide streams work okay for me with older versions of Visual C++,
// but I've had reports of problems. My guess is that it's an
// incompatibility with later versions of windows.
#if defined(BOOST_MSVC) && BOOST_MSVC >= 1700
#define QUICKBOOK_WIDE_STREAMS 1
#endif
#endif

#if !defined(QUICKBOOK_WIDE_PATHS)
#define QUICKBOOK_WIDE_PATHS 0
#endif

#if !defined(QUICKBOOK_WIDE_STREAMS)
#define QUICKBOOK_WIDE_STREAMS 0
#endif

#if !defined(QUICKBOOK_CYGWIN_PATHS)
#define QUICKBOOK_CYGWIN_PATHS 0
#endif

namespace quickbook
{
    namespace detail
    {
        struct conversion_error : std::runtime_error
        {
            conversion_error(char const* m) : std::runtime_error(m) {}
        };

#if QUICKBOOK_WIDE_STREAMS
        typedef std::wstring stream_string;
#else
        typedef std::string stream_string;
#endif

#if QUICKBOOK_WIDE_PATHS || QUICKBOOK_WIDE_STREAMS
        std::string to_utf8(std::wstring const& x);
        std::wstring from_utf8(string_view x);
#endif
    }
}

#endif
