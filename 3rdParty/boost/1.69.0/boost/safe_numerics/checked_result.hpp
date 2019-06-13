#ifndef BOOST_NUMERIC_CHECKED_RESULT
#define BOOST_NUMERIC_CHECKED_RESULT

// MS compatible compilers support #pragma once
#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// contains operations for doing checked aritmetic on NATIVE
// C++ types.
#include <cassert>
#include <type_traits> // is_convertible
#include "exception.hpp"

namespace boost {
namespace safe_numerics {

template<typename R>
struct checked_result {
    const safe_numerics_error m_e;
    const union {
        R m_r;
        char const * m_msg;
    };
    
    // don't permit construction without initial value;
    checked_result() = delete;

    constexpr /*explicit*/ checked_result(const R & r) :
        m_e(safe_numerics_error::success),
        m_r(r)
    {}
    #if 0
    template<typename T>
    constexpr /*explicit*/ checked_result(const T & t) :
        m_e(safe_numerics_error::success),
        m_r(t)
    {}
    #endif
    constexpr /*explicit*/ checked_result(
        safe_numerics_error e,
        const char * msg = ""
    ) :
        m_e(e),
        m_msg(msg)
    {
        assert(m_e != safe_numerics_error::success);
    }
    // permit construct from another checked result type
    template<typename T>
    constexpr /*explicit*/ checked_result(const checked_result<T> & t) :
        m_e(t.m_e)
    {
        static_assert(
            std::is_convertible<T, R>::value,
            "T must be convertible to R"
        );
        if(safe_numerics_error::success == t.m_e)
            m_r = t.m_r;
        else
            m_msg = t.m_msg;
    }
    constexpr bool exception() const {
        return m_e != safe_numerics_error::success;
    }

    // accesors
    constexpr operator R() const {
        // don't assert here.  Let the library catch these errors
        assert(! exception());
        return m_r;
    }
    
    constexpr operator safe_numerics_error () const {
        // note that this is a legitimate operation even when
        // the operation was successful - it will return success
        return m_e;
    }
    constexpr operator const char *() const {
        assert(exception());
        return m_msg;
    }

    // disallow assignment
    checked_result & operator=(const checked_result &) = delete;
};

} // safe_numerics
} // boost

#endif  // BOOST_NUMERIC_CHECKED_RESULT
