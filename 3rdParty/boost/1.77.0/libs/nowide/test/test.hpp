//
//  Copyright (c) 2012 Artyom Beilis (Tonkikh)
//  Copyright (c) 2019-2020 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef BOOST_NOWIDE_LIB_TEST_H_INCLUDED
#define BOOST_NOWIDE_LIB_TEST_H_INCLUDED

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>

#if defined(_MSC_VER) && defined(_CPPLIB_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace boost {
namespace nowide {
    struct test_monitor
    {
        test_monitor()
        {
#if defined(_MSC_VER) && (_MSC_VER > 1310)
            // disable message boxes on assert(), abort()
            ::_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#if defined(_MSC_VER) && defined(_CPPLIB_VER) && defined(_DEBUG)
            // disable message boxes on iterator debugging violations
            _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
        }
    };
} // namespace nowide
} // namespace boost

inline boost::nowide::test_monitor& test_mon()
{
    static boost::nowide::test_monitor instance;
    return instance;
}

/// Function called when a test failed to be able set a breakpoint for debugging
inline void test_failed(const char* expr, const char* file, const int line, const char* function)
{
    std::ostringstream ss;
    ss << "Error " << expr << " at " << file << ':' << line << " in " << function;
    throw std::runtime_error(ss.str());
}

template<typename T, typename U>
inline void test_equal_impl(const T& lhs, const U& rhs, const char* file, const int line, const char* function)
{
    if(lhs == rhs)
        return;
    std::ostringstream ss;
    ss << "[" << lhs << "!=" << rhs << "]";
    test_failed(ss.str().c_str(), file, line, function);
}

#ifdef _MSC_VER
#define DISABLE_CONST_EXPR_DETECTED __pragma(warning(push)) __pragma(warning(disable : 4127))
#define DISABLE_CONST_EXPR_DETECTED_POP __pragma(warning(pop))
#else
#define DISABLE_CONST_EXPR_DETECTED
#define DISABLE_CONST_EXPR_DETECTED_POP
#endif

#define TEST(x)                                            \
    do                                                     \
    {                                                      \
        test_mon();                                        \
        if(x)                                              \
            break;                                         \
        test_failed(#x, __FILE__, __LINE__, __FUNCTION__); \
        DISABLE_CONST_EXPR_DETECTED                        \
    } while(0) DISABLE_CONST_EXPR_DETECTED_POP
#define TEST_EQ(lhs, rhs)                                                \
    do                                                                   \
    {                                                                    \
        test_mon();                                                      \
        test_equal_impl((lhs), (rhs), __FILE__, __LINE__, __FUNCTION__); \
        break;                                                           \
        DISABLE_CONST_EXPR_DETECTED                                      \
    } while(0) DISABLE_CONST_EXPR_DETECTED_POP

#ifndef BOOST_NOWIDE_TEST_NO_MAIN
// Tests should implement this
void test_main(int argc, char** argv, char** env);

int main(int argc, char** argv, char** env)
{
    try
    {
        test_main(argc, argv, env);
    } catch(const std::exception& e)
    {
        std::cerr << "Failed " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
#endif

#endif // #ifndef BOOST_NOWIDE_LIB_TEST_H_INCLUDED
