//  Copyright (c) 2011 Helge Bahmann
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  Verify that definition of the "LOCK_FREE" macros and the
//  "is_lock_free" members is consistent and matches expectations.
//  Also, if any operation is lock-free, then the platform
//  implementation must provide overridden fence implementations.

#include <boost/atomic.hpp>

#include <iostream>
#include <boost/config.hpp>
#include <boost/core/lightweight_test.hpp>

static const char * lock_free_level[] = {
    "never",
    "sometimes",
    "always"
};

template<typename T>
void
verify_lock_free(const char * type_name, int lock_free_macro_val, int lock_free_expect)
{
    BOOST_TEST(lock_free_macro_val >= 0 && lock_free_macro_val <= 2);
    BOOST_TEST(lock_free_macro_val == lock_free_expect);

    boost::atomic<T> value;

    if (lock_free_macro_val == 0)
        BOOST_TEST(!value.is_lock_free());
    if (lock_free_macro_val == 2)
        BOOST_TEST(value.is_lock_free());

    BOOST_TEST(boost::atomic<T>::is_always_lock_free == (lock_free_expect == 2));

    std::cout << "atomic<" << type_name << "> is " << lock_free_level[lock_free_macro_val] << " lock free\n";
}

#if (defined(__GNUC__) || defined(__SUNPRO_CC)) && defined(__i386__)

#define EXPECT_CHAR_LOCK_FREE 2
#define EXPECT_SHORT_LOCK_FREE 2
#define EXPECT_INT_LOCK_FREE 2
#define EXPECT_LONG_LOCK_FREE 2
#if defined(BOOST_ATOMIC_DETAIL_X86_HAS_CMPXCHG8B)
#define EXPECT_LLONG_LOCK_FREE 2
#else
#define EXPECT_LLONG_LOCK_FREE 0
#endif
#define EXPECT_INT128_LOCK_FREE 0
#define EXPECT_POINTER_LOCK_FREE 2
#define EXPECT_BOOL_LOCK_FREE 2

#elif (defined(__GNUC__) || defined(__SUNPRO_CC)) && defined(__x86_64__)

#define EXPECT_CHAR_LOCK_FREE 2
#define EXPECT_SHORT_LOCK_FREE 2
#define EXPECT_INT_LOCK_FREE 2
#define EXPECT_LONG_LOCK_FREE 2
#define EXPECT_LLONG_LOCK_FREE 2
#if defined(BOOST_ATOMIC_DETAIL_X86_HAS_CMPXCHG16B) && (defined(BOOST_HAS_INT128) || !defined(BOOST_NO_ALIGNMENT))
#define EXPECT_INT128_LOCK_FREE 2
#else
#define EXPECT_INT128_LOCK_FREE 0
#endif
#define EXPECT_POINTER_LOCK_FREE 2
#define EXPECT_BOOL_LOCK_FREE 2

#elif defined(__GNUC__) && (defined(__POWERPC__) || defined(__PPC__))

#define EXPECT_CHAR_LOCK_FREE 2
#define EXPECT_CHAR16_T_LOCK_FREE 2
#define EXPECT_CHAR32_T_LOCK_FREE 2
#define EXPECT_WCHAR_T_LOCK_FREE 2
#define EXPECT_SHORT_LOCK_FREE 2
#define EXPECT_INT_LOCK_FREE 2
#define EXPECT_LONG_LOCK_FREE 2
#if defined(__powerpc64__)
#define EXPECT_LLONG_LOCK_FREE 2
#else
#define EXPECT_LLONG_LOCK_FREE 0
#endif
#define EXPECT_INT128_LOCK_FREE 0
#define EXPECT_POINTER_LOCK_FREE 2
#define EXPECT_BOOL_LOCK_FREE 2

#elif defined(__GNUC__) && defined(__alpha__)

#define EXPECT_CHAR_LOCK_FREE 2
#define EXPECT_CHAR16_T_LOCK_FREE 2
#define EXPECT_CHAR32_T_LOCK_FREE 2
#define EXPECT_WCHAR_T_LOCK_FREE 2
#define EXPECT_SHORT_LOCK_FREE 2
#define EXPECT_INT_LOCK_FREE 2
#define EXPECT_LONG_LOCK_FREE 2
#define EXPECT_LLONG_LOCK_FREE 2
#define EXPECT_INT128_LOCK_FREE 0
#define EXPECT_POINTER_LOCK_FREE 2
#define EXPECT_BOOL_LOCK_FREE 2

#elif defined(__GNUC__) &&\
    (\
        defined(__ARM_ARCH_6__)  || defined(__ARM_ARCH_6J__) ||\
        defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) ||\
        defined(__ARM_ARCH_6ZK__) ||\
        defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) ||\
        defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) ||\
        defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_7S__)\
    )

#define EXPECT_CHAR_LOCK_FREE 2
#define EXPECT_SHORT_LOCK_FREE 2
#define EXPECT_INT_LOCK_FREE 2
#define EXPECT_LONG_LOCK_FREE 2
#if !(defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6Z__)\
    || ((defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6ZK__)) && defined(__thumb__)) || defined(__ARM_ARCH_7M__))
#define EXPECT_LLONG_LOCK_FREE 2
#else
#define EXPECT_LLONG_LOCK_FREE 0
#endif
#define EXPECT_INT128_LOCK_FREE 0
#define EXPECT_POINTER_LOCK_FREE 2
#define EXPECT_BOOL_LOCK_FREE 2

#elif defined(__linux__) && defined(__arm__)

#define EXPECT_CHAR_LOCK_FREE 2
#define EXPECT_SHORT_LOCK_FREE 2
#define EXPECT_INT_LOCK_FREE 2
#define EXPECT_LONG_LOCK_FREE 2
#define EXPECT_LLONG_LOCK_FREE 0
#define EXPECT_INT128_LOCK_FREE 0
#define EXPECT_POINTER_LOCK_FREE 2
#define EXPECT_BOOL_LOCK_FREE 2

#elif (defined(__GNUC__) || defined(__SUNPRO_CC)) && (defined(__sparcv8plus) || defined(__sparc_v9__))

#define EXPECT_CHAR_LOCK_FREE 2
#define EXPECT_SHORT_LOCK_FREE 2
#define EXPECT_INT_LOCK_FREE 2
#define EXPECT_LONG_LOCK_FREE 2
#define EXPECT_LLONG_LOCK_FREE 2
#define EXPECT_INT128_LOCK_FREE 0
#define EXPECT_POINTER_LOCK_FREE 2
#define EXPECT_BOOL_LOCK_FREE 2

#elif defined(BOOST_USE_WINDOWS_H) || defined(_WIN32_CE) || defined(BOOST_MSVC) || defined(BOOST_INTEL_WIN) || defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__)

#define EXPECT_CHAR_LOCK_FREE 2
#define EXPECT_SHORT_LOCK_FREE 2
#define EXPECT_INT_LOCK_FREE 2
#define EXPECT_LONG_LOCK_FREE 2
#if defined(_WIN64) || defined(BOOST_ATOMIC_DETAIL_X86_HAS_CMPXCHG8B) || defined(_M_AMD64) || defined(_M_IA64) || (_MSC_VER >= 1700 && (defined(_M_ARM) || defined(_M_ARM64)))
#define EXPECT_LLONG_LOCK_FREE 2
#else
#define EXPECT_LLONG_LOCK_FREE 0
#endif
#define EXPECT_INT128_LOCK_FREE 0
#define EXPECT_POINTER_LOCK_FREE 2
#define EXPECT_BOOL_LOCK_FREE 2

#else

#define EXPECT_CHAR_LOCK_FREE 0
#define EXPECT_SHORT_LOCK_FREE 0
#define EXPECT_INT_LOCK_FREE 0
#define EXPECT_LONG_LOCK_FREE 0
#define EXPECT_LLONG_LOCK_FREE 0
#define EXPECT_INT128_LOCK_FREE 0
#define EXPECT_POINTER_LOCK_FREE 0
#define EXPECT_BOOL_LOCK_FREE 0

#endif

int main(int, char *[])
{
    verify_lock_free<char>("char", BOOST_ATOMIC_CHAR_LOCK_FREE, EXPECT_CHAR_LOCK_FREE);
    verify_lock_free<short>("short", BOOST_ATOMIC_SHORT_LOCK_FREE, EXPECT_SHORT_LOCK_FREE);
    verify_lock_free<int>("int", BOOST_ATOMIC_INT_LOCK_FREE, EXPECT_INT_LOCK_FREE);
    verify_lock_free<long>("long", BOOST_ATOMIC_LONG_LOCK_FREE, EXPECT_LONG_LOCK_FREE);
#ifdef BOOST_HAS_LONG_LONG
    verify_lock_free<long long>("long long", BOOST_ATOMIC_LLONG_LOCK_FREE, EXPECT_LLONG_LOCK_FREE);
#endif
#ifdef BOOST_HAS_INT128
    verify_lock_free<boost::int128_type>("int128", BOOST_ATOMIC_INT128_LOCK_FREE, EXPECT_INT128_LOCK_FREE);
#endif
    verify_lock_free<void *>("void *", BOOST_ATOMIC_POINTER_LOCK_FREE, EXPECT_SHORT_LOCK_FREE);
    verify_lock_free<bool>("bool", BOOST_ATOMIC_BOOL_LOCK_FREE, EXPECT_BOOL_LOCK_FREE);

#ifndef BOOST_ATOMIC_NO_FLOATING_POINT

    verify_lock_free<float>("float", BOOST_ATOMIC_FLOAT_LOCK_FREE,
        sizeof(float) == 1 ? EXPECT_CHAR_LOCK_FREE : (sizeof(float) == 2 ? EXPECT_SHORT_LOCK_FREE :
        (sizeof(float) <= 4 ? EXPECT_INT_LOCK_FREE : (sizeof(float) <= 8 ? EXPECT_LLONG_LOCK_FREE : (sizeof(float) <= 16 ? EXPECT_INT128_LOCK_FREE : 0)))));

    verify_lock_free<double>("double", BOOST_ATOMIC_DOUBLE_LOCK_FREE,
        sizeof(double) == 1 ? EXPECT_CHAR_LOCK_FREE : (sizeof(double) == 2 ? EXPECT_SHORT_LOCK_FREE :
        (sizeof(double) <= 4 ? EXPECT_INT_LOCK_FREE : (sizeof(double) <= 8 ? EXPECT_LLONG_LOCK_FREE : (sizeof(double) <= 16 ? EXPECT_INT128_LOCK_FREE : 0)))));

    verify_lock_free<long double>("long double", BOOST_ATOMIC_LONG_DOUBLE_LOCK_FREE,
        sizeof(long double) == 1 ? EXPECT_CHAR_LOCK_FREE : (sizeof(long double) == 2 ? EXPECT_SHORT_LOCK_FREE :
        (sizeof(long double) <= 4 ? EXPECT_INT_LOCK_FREE : (sizeof(long double) <= 8 ? EXPECT_LLONG_LOCK_FREE : (sizeof(long double) <= 16 ? EXPECT_INT128_LOCK_FREE : 0)))));

#ifdef BOOST_HAS_FLOAT128
    verify_lock_free<boost::float128_type>("float128", BOOST_ATOMIC_INT128_LOCK_FREE, EXPECT_INT128_LOCK_FREE);
#endif

#endif // BOOST_ATOMIC_NO_FLOATING_POINT

    bool any_lock_free =
        BOOST_ATOMIC_CHAR_LOCK_FREE > 0 ||
        BOOST_ATOMIC_SHORT_LOCK_FREE > 0 ||
        BOOST_ATOMIC_INT_LOCK_FREE > 0 ||
        BOOST_ATOMIC_LONG_LOCK_FREE > 0 ||
        BOOST_ATOMIC_LLONG_LOCK_FREE > 0 ||
        BOOST_ATOMIC_BOOL_LOCK_FREE > 0;

    BOOST_TEST(!any_lock_free || BOOST_ATOMIC_THREAD_FENCE > 0);

    return boost::report_errors();
}
