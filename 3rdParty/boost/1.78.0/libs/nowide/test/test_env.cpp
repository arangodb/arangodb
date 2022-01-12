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

void test_main(int, char**, char**) // coverity [root_function]
{
    std::string example = "\xd7\xa9-\xd0\xbc-\xce\xbd";
    std::string envVar = "BOOST_TEST2=" + example + "x";

    TEST(boost::nowide::setenv("BOOST_TEST1", example.c_str(), 1) == 0);
    TEST(boost::nowide::getenv("BOOST_TEST1"));
    TEST(boost::nowide::getenv("BOOST_TEST1") == example);
    TEST(boost::nowide::setenv("BOOST_TEST1", "xx", 0) == 0);
    TEST(boost::nowide::getenv("BOOST_TEST1") == example);
    char* penv = const_cast<char*>(envVar.c_str());
    TEST(boost::nowide::putenv(penv) == 0);
    TEST(boost::nowide::getenv("BOOST_TEST2"));
    TEST(boost::nowide::getenv("BOOST_TEST_INVALID") == 0);
    TEST(boost::nowide::getenv("BOOST_TEST2") == example + "x");
#ifdef BOOST_WINDOWS
    // Passing a variable without an equals sign (before \0) is an error
    // But GLIBC has an extension that unsets the env var instead
    std::string envVar2 = "BOOST_TEST1SOMEGARBAGE=";
    // End the string before the equals sign -> Expect fail
    envVar2[strlen("BOOST_TEST1")] = '\0';
    char* penv2 = const_cast<char*>(envVar2.c_str());
    TEST(boost::nowide::putenv(penv2) == -1);
    TEST(boost::nowide::getenv("BOOST_TEST1"));
    TEST(boost::nowide::getenv("BOOST_TEST1") == example);
#endif
}
