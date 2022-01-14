//
//  Copyright (c) 2012 Artyom Beilis (Tonkikh)
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/nowide/cstdlib.hpp>
#include "test.hpp"
#include <cstring>

#if defined(BOOST_NOWIDE_TEST_INCLUDE_WINDOWS) && defined(BOOST_WINDOWS)
#include <windows.h>
#endif

// "Safe" strcpy version with NULL termination to make MSVC runtime happy
// which warns when using strncpy
template<size_t size>
void strcpy_safe(char (&dest)[size], const char* src)
{
    size_t len = std::strlen(src);
    if(len >= size)
        len = size - 1u;
    std::memcpy(dest, src, len);
    dest[len] = 0;
}

// coverity [root_function]
void test_main(int, char**, char**)
{
    std::string example = "\xd7\xa9-\xd0\xbc-\xce\xbd";
    char penv[256] = {0};
    strcpy_safe(penv, ("BOOST_TEST2=" + example + "x").c_str());

    TEST(boost::nowide::setenv("BOOST_TEST1", example.c_str(), 1) == 0);
    TEST(boost::nowide::getenv("BOOST_TEST1"));
    TEST(boost::nowide::getenv("BOOST_TEST1") == example);
    TEST(boost::nowide::setenv("BOOST_TEST1", "xx", 0) == 0);
    TEST(boost::nowide::getenv("BOOST_TEST1") == example);
    TEST(boost::nowide::putenv(penv) == 0);
    TEST(boost::nowide::getenv("BOOST_TEST2"));
    TEST(boost::nowide::getenv("BOOST_TEST_INVALID") == 0);
    TEST(boost::nowide::getenv("BOOST_TEST2") == example + "x");
#ifdef BOOST_WINDOWS
    // Passing a variable without an equals sign (before \0) is an error
    // But GLIBC has an extension that unsets the env var instead
    char penv2[256] = {0};
    const char* sPenv2 = "BOOST_TEST1SOMEGARBAGE=";
    strcpy_safe(penv2, sPenv2);
    // End the string before the equals sign -> Expect fail
    penv2[strlen("BOOST_TEST1")] = '\0';
    TEST(boost::nowide::putenv(penv2) == -1);
    TEST(boost::nowide::getenv("BOOST_TEST1"));
    TEST(boost::nowide::getenv("BOOST_TEST1") == example);
#endif
}
