// Copyright (c) 2009-2014 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#ifndef BOOST_CONVERT_TEST_HPP
#define BOOST_CONVERT_TEST_HPP

#include <boost/convert/detail/forward.hpp>
#include <boost/make_default.hpp>
#include <boost/static_assert.hpp>
#include <string>
#include <istream>
#include <string.h> // For strlen, strcmp, memcpy
#include <memory.h> // Is needed for 'memset'
#include <stdio.h>

#if defined(_MSC_VER)
#   pragma warning(disable: 4189) // local variable is initialized but not referenced.
#   pragma warning(disable: 4127) // conditional expression is constant.
#   pragma warning(disable: 4100) // unreferenced formal parameter.
#   pragma warning(disable: 4714) // marked as __forceinline not #endif
#   pragma warning(disable: 4706)
#   pragma warning(disable: 4005)
#endif

//[change_declaration
struct change
{
    enum value_type { no, up, dn };

    change(value_type v =no) : value_(v) {}
    bool operator==(change v) const { return value_ == v.value_; }
    value_type value() const { return value_; }

    private: value_type value_;
};
//]
//[change_stream_operators
std::istream& operator>>(std::istream& stream, change& chg)
{
    std::string str; stream >> str;

    /**/ if (str == "up") chg = change::up;
    else if (str == "dn") chg = change::dn;
    else if (str == "no") chg = change::no;
    else stream.setstate(std::ios_base::failbit);

    return stream;
}

std::ostream& operator<<(std::ostream& stream, change const& chg)
{
    return stream << (chg == change::up ? "up" : chg == change::dn ? "dn" : "no");
}
//]
//[change_convert_operators
inline void operator>>(change chg, boost::optional<std::string>& str)
{
    str = chg == change::up ? "up" : chg == change::dn ? "dn" : "no";
}

inline void operator>>(std::string const& str, boost::optional<change>& chg)
{
    /**/ if (str == "up") chg = change::up;
    else if (str == "dn") chg = change::dn;
    else if (str == "no") chg = change::no;
}
//]
//[direction_declaration
struct direction
{
    // Note: the class does NOT have the default constructor.

    enum value_type { up, dn };

    direction(value_type value) : value_(value) {}
    bool operator==(direction that) const { return value_ == that.value_; }
    value_type value() const { return value_; }

    private: value_type value_;
};
//]
//[direction_stream_operators
std::istream& operator>>(std::istream& stream, direction& dir)
{
    std::string str; stream >> str;

    /**/ if (str == "up") dir = direction::up;
    else if (str == "dn") dir = direction::dn;
    else stream.setstate(std::ios_base::failbit);

    return stream;
}
std::ostream& operator<<(std::ostream& stream, direction const& dir)
{
    return stream << (dir.value() == direction::up ? "up" : "dn");
}
//]
//[direction_declaration_make_default
namespace boost
{
    template<> inline direction make_default<direction>()
    {
        return direction(direction::up);
    }
}
//]
// Quick and dirty small-string implementation for performance tests.
//[my_string_declaration
struct my_string
{
    typedef my_string              this_type;
    typedef char                  value_type;
    typedef value_type*             iterator;
    typedef value_type const* const_iterator;

    my_string ();
    my_string (const_iterator, const_iterator =0);

    char const*    c_str () const { return storage_; }
    const_iterator begin () const { return storage_; }
    const_iterator   end () const { return storage_ + strlen(storage_); }
    this_type& operator= (char const*);

    private:

    static size_t const size_ = 12;
    char storage_[size_];
};
//]
inline
my_string::my_string()
{
    storage_[0] = 0;
}

inline
my_string::my_string(const_iterator beg, const_iterator end)
{
    std::size_t const sz = end ? (end - beg) : strlen(beg);

    BOOST_ASSERT(sz < size_);
    memcpy(storage_, beg, sz); storage_[sz] = 0;
}

inline
my_string&
my_string::operator=(char const* str)
{
    BOOST_ASSERT(strlen(str) < size_);
    strcpy(storage_, str);
    return *this;
}

inline bool operator==(char const* s1, my_string const& s2) { return strcmp(s1, s2.c_str()) == 0; }
inline bool operator==(my_string const& s1, char const* s2) { return strcmp(s2, s1.c_str()) == 0; }

namespace test
{
    struct cnv
    {
#if defined(__QNXNTO__)
        static bool const     is_qnx = true;
#else
        static bool const     is_qnx = false;
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900
        static bool const     is_msc = true;
        static bool const is_old_msc = true;
#elif defined(_MSC_VER)
        static bool const     is_msc = true;
        static bool const is_old_msc = false;
#elif defined(__MINGW32__)
        static bool const     is_msc = true;
        static bool const is_old_msc = true;
#else
        static bool const     is_msc = false;
        static bool const is_old_msc = false;
#endif
    };
}

#endif // BOOST_CONVERT_TEST_HPP
