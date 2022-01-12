//
//  Copyright (c) 2012 Artyom Beilis (Tonkikh)
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <boost/nowide/cstdlib.hpp>

#include <boost/nowide/args.hpp>
#include <boost/nowide/utf/convert.hpp>
#include <boost/nowide/utf/utf.hpp>
#include "test.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

bool is_ascii(const std::string& s)
{
    return std::find_if(s.begin(), s.end(), [](const char c) { return static_cast<unsigned char>(c) > 0x7F; })
           == s.end();
}

std::string replace_non_ascii(const std::string& s) // LCOV_EXCL_LINE
{
    std::string::const_iterator it = s.begin();
    namespace utf = boost::nowide::utf;
    using utf8 = utf::utf_traits<char>;
    std::string result;
    result.reserve(s.size());
    while(it != s.end())
    {
        utf::code_point c = utf8::decode(it, s.end());
        TEST(c != utf::illegal && c != utf::incomplete);
        if(c > 0x7F)
            c = '?'; // WinAPI seems to do this
        result.push_back(static_cast<char>(c));
    }
    return result;
}

void compare_string_arrays(char** main_val, char** utf8_val, bool sort)
{
    std::vector<std::string> vec_main, vec_utf8;
    for(; *main_val; ++main_val)
        vec_main.push_back(std::string(*main_val));
    for(; *utf8_val; ++utf8_val)
        vec_utf8.push_back(std::string(*utf8_val));
    // Same number of strings
    TEST_EQ(vec_main.size(), vec_utf8.size());
    if(sort)
    {
        // Order doesn't matter
        std::sort(vec_main.begin(), vec_main.end());
        std::sort(vec_utf8.begin(), vec_utf8.end());
    }
    for(size_t i = 0; i < vec_main.size(); ++i)
    {
        // Skip strings with non-ascii chars
        if(is_ascii(vec_main[i]) && vec_main[i] != vec_utf8[i])
            TEST_EQ(vec_main[i], replace_non_ascii(vec_utf8[i]));
    }
}

void compare_getenv(char** env)
{
    // For all all variables in env check against getenv
    for(char** e = env; *e != 0; e++)
    {
        const char* key_begin = *e;
        const char* key_end = strchr(key_begin, '=');
        TEST(key_end);
        std::string key = std::string(key_begin, key_end);
        const char* std_value = std::getenv(key.c_str());
        const char* bnw_value = boost::nowide::getenv(key.c_str());
        assert(std_value);
        TEST(bnw_value);
        // Compare only if ascii
        if(is_ascii(std_value) && std::string(std_value) != std::string(bnw_value))
            TEST_EQ(std_value, replace_non_ascii(bnw_value));
    }
}

const std::string example = "\xd7\xa9-\xd0\xbc-\xce\xbd";

void run_child(int argc, char** argv, char** env)
{
    // Test arguments
    TEST_EQ(argc, 2);
    TEST_EQ(argv[1], example);
    TEST(argv[2] == NULL);

    // Test getenv
    TEST(boost::nowide::getenv("BOOST_NOWIDE_TEST"));
    TEST_EQ(boost::nowide::getenv("BOOST_NOWIDE_TEST"), example);
    TEST(boost::nowide::getenv("BOOST_NOWIDE_TEST_NONE") == NULL);
    // Empty variables are unreliable on windows, hence skip. E.g. using "set FOO=" unsets FOO
#ifndef BOOST_WINDOWS
    TEST(boost::nowide::getenv("BOOST_NOWIDE_EMPTY"));
    TEST_EQ(boost::nowide::getenv("BOOST_NOWIDE_EMPTY"), std::string());
#endif // !_WIN32

    // This must be contained in env
    std::string sample = "BOOST_NOWIDE_TEST=" + example;
    bool found = false;
    for(char** e = env; *e != 0; e++)
    {
        if(*e == sample)
            found = true;
    }
    TEST(found);

    std::cout << "Subprocess ok" << std::endl;
}

void run_parent(const char* exe_path)
{
    TEST(boost::nowide::system(nullptr) != 0);
    const std::string command = "\"" + std::string(exe_path) + "\" " + example;
#if BOOST_NOWIDE_TEST_USE_NARROW
    TEST_EQ(boost::nowide::setenv("BOOST_NOWIDE_TEST", example.c_str(), 1), 0);
    TEST_EQ(boost::nowide::setenv("BOOST_NOWIDE_TEST_NONE", example.c_str(), 1), 0);
    TEST_EQ(boost::nowide::unsetenv("BOOST_NOWIDE_TEST_NONE"), 0);
    TEST_EQ(boost::nowide::setenv("BOOST_NOWIDE_EMPTY", "", 1), 0);
    TEST(boost::nowide::getenv("BOOST_NOWIDE_EMPTY"));
    TEST_EQ(boost::nowide::system(command.c_str()), 0);
    std::cout << "Parent ok" << std::endl;
#else
    const std::wstring envVar = L"BOOST_NOWIDE_TEST=" + boost::nowide::widen(example);
    TEST_EQ(_wputenv(envVar.c_str()), 0);
    const std::wstring wcommand = boost::nowide::widen(command);
    TEST_EQ(_wsystem(wcommand.c_str()), 0);
    std::cout << "Wide Parent ok" << std::endl;
#endif
}

void test_main(int argc, char** argv, char** env) // coverity [root_function]
{
    const int old_argc = argc;
    char** old_argv = argv;
    char** old_env = env;
    {
        boost::nowide::args _(argc, argv, env);
        TEST_EQ(argc, old_argc);
        std::cout << "Checking arguments" << std::endl;
        compare_string_arrays(old_argv, argv, false);
        std::cout << "Checking env" << std::endl;
        compare_string_arrays(old_env, env, true);
        compare_getenv(env);
    }
    // When `args` is destructed the old values must be restored
    TEST_EQ(argc, old_argc);
    TEST(argv == old_argv);
    TEST(env == old_env);

    boost::nowide::args a(argc, argv, env);
    if(argc == 1)
        run_parent(argv[0]);
    else
        run_child(argc, argv, env);
}
