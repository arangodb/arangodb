#ifndef BOOST_SERIALIZATION_TEST_A_HPP
#define BOOST_SERIALIZATION_TEST_A_HPP

// MS compatible compilers support #pragma once
#if defined(_MSC_VER)
# pragma once
#endif

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// A.hpp    simple class test

// (C) Copyright 2002 Robert Ramey - http://www.rrsd.com .
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org for updates, documentation, and revision history.

#include <ostream> // for friend output operators
#include <cstddef> // size_t
#include <boost/config.hpp>

#if defined(BOOST_NO_STDC_NAMESPACE)
namespace std{
    using ::size_t;
}
#endif

#include <boost/limits.hpp>
#include <boost/cstdint.hpp>

#include <boost/serialization/access.hpp>
#include <boost/serialization/string.hpp>

#if defined(A_IMPORT)
    #define A_DLL_DECL BOOST_SYMBOL_IMPORT
    #pragma message("A imported")
#elif defined(A_EXPORT)
    #define A_DLL_DECL BOOST_SYMBOL_EXPORT
    #pragma message ("A exported")
#endif

#ifndef A_DLL_DECL
    #define A_DLL_DECL
#endif

class A_DLL_DECL A {
private:
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(
        Archive &ar,
        const unsigned int /* file_version */
    );
    bool b;
    #ifndef BOOST_NO_INT64_T
    boost::int64_t f;
    boost::uint64_t g;
    #endif
    enum h {
        i = 0,
        j,
        k
    } l;
    std::size_t m;
    signed long n;
    unsigned long o;
    signed  short p;
    unsigned short q;
    #ifndef BOOST_NO_CWCHAR
    wchar_t r;
    #endif
    char c;
    signed char s;
    unsigned char t;
    signed int u;
    unsigned int v;
    float w;
    double x;
    std::string y;
    #ifndef BOOST_NO_STD_WSTRING
    std::wstring z;
    #endif
public:
    A();
    bool check_equal(const A &rhs) const;
    bool operator==(const A &rhs) const;
    bool operator!=(const A &rhs) const;
    bool operator<(const A &rhs) const; // used by less
    // hash function for class A
    operator std::size_t () const;
    friend std::ostream & operator<<(std::ostream & os, A const & a);
};

#endif // BOOST_SERIALIZATION_TEST_A_HPP
