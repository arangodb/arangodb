
// Copyright 2017 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Not a test, just a small program to write out configuration info

#include <boost/container_hash/hash.hpp>
#include <iostream>
#include <algorithm>

#if defined(BOOST_MSVC)

struct msvc_version {
    unsigned version;
    char const* description;

    friend bool operator<(msvc_version const& v1, msvc_version const& v2) {
        return v1.version < v2.version;
    }
};

void write_compiler_info() {
    // From:
    // https://en.wikipedia.org/wiki/Microsoft_Visual_C%2B%2B
    // https://blogs.msdn.microsoft.com/vcblog/2017/11/15/side-by-side-minor-version-msvc-toolsets-in-visual-studio-2017/
    msvc_version versions[] = {
        {0, "Old Visual C++"},
        {1000, "Visual C++ 4.x, VS4.0?"},
        {1100, "Visual C++ 5.0, VS97"},
        {1200, "Visual C++ 6.0, VS6.0"},
        {1300, "Visual C++ 7.0, VS.NET 2002"},
        {1310, "Visual C++ 7.1, VS.NET 2003"},
        {1400, "Visual C++ 8.0, VS2005"},
        {1500, "Visual C++ 9.0, VS2008"},
        {1600, "Visual C++ 10.0, VS2010"},
        {1700, "Visual C++ 11.0, VS2012"},
        {1800, "Visual C++ 12.0, VS2013"},
        {1900, "Visual C++ 14.00, VS2015"},
        {1910, "Visual C++ 14.10, VS2017 15.1/2"},
        {1911, "Visual C++ 14.11, VS2017 15.3/4"},
        {1912, "Visual C++ 14.12, VS2017 15.5"},
        {1913, "Visual C++ 14.13, VS2017 15.6"}
    };

    msvc_version msvc = { BOOST_MSVC, "" };
    msvc_version* v = std::upper_bound(versions,
        versions + sizeof(versions) / sizeof(*versions),
        msvc) - 1;
    unsigned difference = msvc.version - v->version;

    std::cout << v->description << std::endl;
    if (difference) {
        std::cout << "+" << difference << std::endl;
    }
}

#else

void write_compiler_info() {
}

#endif

int main() {
    write_compiler_info();

#if defined(__cplusplus)
    std::cout << "__cplusplus: "
        << __cplusplus
        << std::endl;
#endif

    std::cout << "BOOST_HASH_CXX17: "
        << BOOST_HASH_CXX17
        << std::endl;

    std::cout << "BOOST_HASH_HAS_STRING_VIEW: "
        << BOOST_HASH_HAS_STRING_VIEW
        << std::endl;

    std::cout << "BOOST_HASH_HAS_OPTIONAL: "
        << BOOST_HASH_HAS_OPTIONAL
        << std::endl;

    std::cout << "BOOST_HASH_HAS_VARIANT: "
        << BOOST_HASH_HAS_VARIANT
        << std::endl;

#if defined(BOOST_NO_CXX11_HDR_TYPEINDEX)
    std::cout << "No <typeindex>" << std::endl;
#else
    std::cout << "<typeindex>" << std::endl;
#endif

#if defined(BOOST_NO_CXX11_HDR_SYSTEM_ERROR)
    std::cout << "No <system_error>" << std::endl;
#else
    std::cout << "<system_error>" << std::endl;
#endif

}
