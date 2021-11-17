//
//  Copyright (c) 2012 Artyom Beilis (Tonkikh)
//  Copyright (c) 2020 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#define BOOST_NOWIDE_SOURCE

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#elif(defined(__MINGW32__) || defined(__CYGWIN__)) && defined(__STRICT_ANSI__)
// Need the _w* functions which are extensions on MinGW/Cygwin
#undef __STRICT_ANSI__
#endif

#include <boost/nowide/cstdio.hpp>
#include <boost/nowide/stackstring.hpp>

namespace boost {
namespace nowide {
    namespace detail {
        FILE* wfopen(const wchar_t* filename, const wchar_t* mode)
        {
#ifdef BOOST_WINDOWS
            // coverity[var_deref_model]
            return ::_wfopen(filename, mode);
#else
            const stackstring name(filename);
            const short_stackstring smode2(mode);
            return std::fopen(name.get(), smode2.get());
#endif
        }
    } // namespace detail

#ifdef BOOST_WINDOWS
    ///
    /// \brief Same as freopen but file_name and mode are UTF-8 strings
    ///
    FILE* freopen(const char* file_name, const char* mode, FILE* stream)
    {
        const wstackstring wname(file_name);
        const wshort_stackstring wmode(mode);
        return _wfreopen(wname.get(), wmode.get(), stream);
    }
    ///
    /// \brief Same as fopen but file_name and mode are UTF-8 strings
    ///
    FILE* fopen(const char* file_name, const char* mode)
    {
        const wstackstring wname(file_name);
        const wshort_stackstring wmode(mode);
        return detail::wfopen(wname.get(), wmode.get());
    }
    ///
    /// \brief Same as rename but old_name and new_name are UTF-8 strings
    ///
    int rename(const char* old_name, const char* new_name)
    {
        const wstackstring wold(old_name), wnew(new_name);
        return _wrename(wold.get(), wnew.get());
    }
    ///
    /// \brief Same as rename but name is UTF-8 string
    ///
    int remove(const char* name)
    {
        const wstackstring wname(name);
        return _wremove(wname.get());
    }
#endif
} // namespace nowide
} // namespace boost
