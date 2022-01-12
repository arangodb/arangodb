//
//  Copyright (c) 2020 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at  http://www.boost.org/LICENSE_1_0.txt)
//

#define BOOST_NOWIDE_SOURCE

#if(defined(__MINGW32__) || defined(__CYGWIN__)) && defined(__STRICT_ANSI__)
// Need the _w* functions which are extensions on MinGW/Cygwin
#undef __STRICT_ANSI__
#endif

#include <boost/nowide/config.hpp>

#if defined(BOOST_WINDOWS)

#include <boost/nowide/stackstring.hpp>
#include <boost/nowide/stat.hpp>
#include <errno.h>

namespace boost {
namespace nowide {
    namespace detail {
        int stat(const char* path, posix_stat_t* buffer, size_t buffer_size)
        {
            if(sizeof(*buffer) != buffer_size)
            {
                errno = EINVAL;
                return EINVAL;
            }
            const wstackstring wpath(path);
            return _wstat(wpath.get(), buffer);
        }
    } // namespace detail
    int stat(const char* path, stat_t* buffer)
    {
        const wstackstring wpath(path);
        return _wstat64(wpath.get(), buffer);
    }
} // namespace nowide
} // namespace boost

#endif
