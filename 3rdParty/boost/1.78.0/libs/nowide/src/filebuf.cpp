//
//  Copyright (c) 2020 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#define BOOST_NOWIDE_SOURCE

#ifdef BOOST_NOWIDE_NO_LFS
#define BOOST_NOWIDE_FTELL ::ftell
#define BOOST_NOWIDE_FSEEK ::fseek
#define BOOST_NOWIDE_OFF_T long
#elif defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MINGW32__)
#define BOOST_NOWIDE_FTELL _ftelli64
#define BOOST_NOWIDE_FSEEK _fseeki64
#define BOOST_NOWIDE_OFF_T int64_t
#else
// IMPORTANT: Have these defines BEFORE any #includes
//            and make sure changes by those macros don't leak into the public interface
// Make LFS functions available
#define _LARGEFILE_SOURCE
// Make off_t 64 bits if the macro isn't set
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#define BOOST_NOWIDE_FTELL ftello
#define BOOST_NOWIDE_FSEEK fseeko
#define BOOST_NOWIDE_OFF_T off_t
#endif

#include <boost/nowide/filebuf.hpp>
#include <cassert>
#include <cstdint>
#include <limits>
#include <stdio.h>
#include <type_traits>

namespace boost {
namespace nowide {
    namespace detail {

        template<typename T, typename U>
        constexpr bool is_in_range(U value)
        {
            static_assert(std::is_signed<T>::value == std::is_signed<U>::value,
                          "Mixed sign comparison can lead to problems below");
            // coverity[result_independent_of_operands]
            return value >= std::numeric_limits<T>::min() && value <= std::numeric_limits<T>::max();
        }

        template<typename T, typename U>
        T cast_if_valid_or_minus_one(U value)
        {
            return is_in_range<T>(value) ? static_cast<T>(value) : T(-1);
        }

        std::streampos ftell(FILE* file)
        {
            const auto pos = BOOST_NOWIDE_FTELL(file);
            // Note that this is used in seekoff for which the standard states:
            // On success, it returns the new absolute position the internal position pointer points to after the call,
            // if representable [...] [or] the function returns pos_type(off_type(-1)). Hence we do a range check first,
            // then cast or return failure instead of silently truncating
            return cast_if_valid_or_minus_one<std::streamoff>(pos);
        }

        int fseek(FILE* file, std::streamoff offset, int origin)
        {
            // Similar to above: If the value of offset can't fit inside target type
            // don't silently truncate but fail right away
            if(!is_in_range<BOOST_NOWIDE_OFF_T>(offset))
                return -1;
            return BOOST_NOWIDE_FSEEK(file, static_cast<BOOST_NOWIDE_OFF_T>(offset), origin);
        }
    } // namespace detail
} // namespace nowide
} // namespace boost
