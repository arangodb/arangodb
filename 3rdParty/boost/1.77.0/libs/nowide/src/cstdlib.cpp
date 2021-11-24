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

#include <boost/nowide/cstdlib.hpp>

#if !defined(BOOST_WINDOWS)
namespace boost {
namespace nowide {
    int setenv(const char* key, const char* value, int overwrite)
    {
        return ::setenv(key, value, overwrite);
    }

    int unsetenv(const char* key)
    {
        return ::unsetenv(key);
    }

    int putenv(char* string)
    {
        return ::putenv(string);
    }
} // namespace nowide
} // namespace boost
#else
#include <boost/nowide/stackstring.hpp>
#include <vector>
#include <windows.h>

namespace boost {
namespace nowide {
    char* getenv(const char* key)
    {
        static stackstring value;

        const wshort_stackstring name(key);

        static const size_t buf_size = 64;
        wchar_t buf[buf_size];
        std::vector<wchar_t> tmp;
        wchar_t* ptr = buf;
        size_t n = GetEnvironmentVariableW(name.get(), buf, buf_size);
        if(n == 0 && GetLastError() == 203) // ERROR_ENVVAR_NOT_FOUND
            return 0;
        if(n >= buf_size)
        {
            tmp.resize(n + 1, L'\0');
            n = GetEnvironmentVariableW(name.get(), &tmp[0], static_cast<unsigned>(tmp.size() - 1));
            // The size may have changed
            if(n >= tmp.size() - 1)
                return 0;
            ptr = &tmp[0];
        }
        value.convert(ptr);
        return value.get();
    }

    int setenv(const char* key, const char* value, int overwrite)
    {
        const wshort_stackstring name(key);
        if(!overwrite)
        {
            wchar_t unused[2];
            if(GetEnvironmentVariableW(name.get(), unused, 2) != 0 || GetLastError() != 203) // ERROR_ENVVAR_NOT_FOUND
                return 0;
        }
        const wstackstring wval(value);
        if(SetEnvironmentVariableW(name.get(), wval.get()))
            return 0;
        return -1;
    }

    int unsetenv(const char* key)
    {
        const wshort_stackstring name(key);
        if(SetEnvironmentVariableW(name.get(), 0))
            return 0;
        return -1;
    }

    int putenv(char* string)
    {
        const char* key = string;
        const char* key_end = string;
        while(*key_end != '=' && *key_end != '\0')
            key_end++;
        if(*key_end == '\0')
            return -1;
        const wshort_stackstring wkey(key, key_end);
        const wstackstring wvalue(key_end + 1);

        if(SetEnvironmentVariableW(wkey.get(), wvalue.get()))
            return 0;
        return -1;
    }

    int system(const char* cmd)
    {
        if(!cmd)
            return _wsystem(0);
        const wstackstring wcmd(cmd);
        return _wsystem(wcmd.get());
    }
} // namespace nowide
} // namespace boost
#endif
