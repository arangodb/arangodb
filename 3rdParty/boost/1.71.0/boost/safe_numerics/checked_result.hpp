#ifndef BOOST_NUMERIC_CHECKED_RESULT
#define BOOST_NUMERIC_CHECKED_RESULT

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

    // note: I implemented the following non-default copy and move
    // constructors because I thought I needed to do this in order
    // to make them constexpr.  Turns out though that doing this creates
    // a syntax error because the assignment results in error due
    // to assignment "outside of object lifetime".  I think this could
    // be addressed by replacing the anonymous union above with a
    // named union.  This would create some syntax changes which would
    // ripple through some parts of th program.  So for now, we'll just
    // rely on the default copy and move constructors.
    #if 0
    // copy constructor
    constexpr /*explicit*/ checked_result(const checked_result & r) noexpect :
        m_e(r.m_e)
    {
        if(safe_numerics_error::success == r.m_e)
            m_r = r.m_r;
        else
            m_msg = r.m_msg;
    }

    // move constructor
    constexpr /*explicit*/ checked_result(checked_result && r) noexcept :
        m_e(r.m_e)
    {
        if(safe_numerics_error::success == r.m_e)
            m_r = r.m_r;
        else
            m_msg = r.m_msg;
    }
    #endif
    checked_result(const checked_result & r) = default;
    checked_result(checked_result && r) = default;
    
    constexpr /*explicit*/ checked_result(const R & r) :
        m_e(safe_numerics_error::success),
        m_r(r)
    {}
    #if 0
    template<typename T>
    constexpr /*explicit*/ checked_result(const T & t) noexcept :
        m_e(safe_numerics_error::success),
        m_r(t)
    {}
    #endif
    constexpr /*explicit*/ checked_result(
        const safe_numerics_error & e,
        const char * msg = ""
    )  noexcept :
        m_e(e),
        m_msg(msg)
    {
        assert(m_e != safe_numerics_error::success);
    }
    // permit construct from another checked result type
    template<typename T>
    constexpr /*explicit*/ checked_result(const checked_result<T> & t) noexcept :
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
    constexpr operator R() const  noexcept{
        // don't assert here.  Let the library catch these errors
        assert(! exception());
        return m_r;
    }
    
    constexpr operator safe_numerics_error () const  noexcept{
        // note that this is a legitimate operation even when
        // the operation was successful - it will return success
        return m_e;
    }
    constexpr operator const char *() const  noexcept{
        assert(exception());
        return m_msg;
    }

    // disallow assignment
    checked_result & operator=(const checked_result &) = delete;
};

#if 0
template<typename R>
constexpr checked_result<R> make_checked_result(
    const safe_numerics_error & e,
    char const * const & m
)  noexcept {
    return checked_result<R>(e, m);
}
#endif

template <class R>
class make_checked_result {
public:
    template<safe_numerics_error E>
    constexpr static checked_result<R> invoke(
        char const * const & m
    ) noexcept {
        return checked_result<R>(E, m);
    }
};

} // safe_numerics
} // boost

#endif  // BOOST_NUMERIC_CHECKED_RESULT
