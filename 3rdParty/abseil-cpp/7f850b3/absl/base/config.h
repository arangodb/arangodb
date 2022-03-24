//
// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: config.h
// -----------------------------------------------------------------------------
//
// This header file defines a set of macros for checking the presence of
// important compiler and platform features. Such macros can be used to
// produce portable code by parameterizing compilation based on the presence or
// lack of a given feature.
//
// We define a "feature" as some interface we wish to program to: for example,
// a library function or system call. A value of `1` indicates support for
// that feature; any other value indicates the feature support is undefined.
//
// Example:
//
// Suppose a programmer wants to write a program that uses the 'mmap()' system
// call. The Abseil macro for that feature (`ABSL_HAVE_MMAP`) allows you to
// selectively include the `mmap.h` header and bracket code using that feature
// in the macro:
//
//   #include "absl/base/config.h"
//
//   #ifdef ABSL_HAVE_MMAP
//   #include "sys/mman.h"
//   #endif  //ABSL_HAVE_MMAP
//
//   ...
//   #ifdef ABSL_HAVE_MMAP
//   void *ptr = mmap(...);
//   ...
//   #endif  // ABSL_HAVE_MMAP

#ifndef ABSL_BASE_CONFIG_H_
#define ABSL_BASE_CONFIG_H_

// Included for the __GLIBC__ macro (or similar macros on other systems).
#include <limits.h>

#ifdef __cplusplus
// Included for __GLIBCXX__, _LIBCPP_VERSION
#include <cstddef>
#endif  // __cplusplus

#if defined(__APPLE__)
// Included for TARGET_OS_IPHONE, __IPHONE_OS_VERSION_MIN_REQUIRED,
// __IPHONE_8_0.
#include <Availability.h>
#include <TargetConditionals.h>
#endif

#include "absl/base/options.h"
#include "absl/base/policy_checks.h"

// Abseil long-term support (LTS) releases will define
// `ABSL_LTS_RELEASE_VERSION` to the integer representing the date string of the
// LTS release version, and will define `ABSL_LTS_RELEASE_PATCH_LEVEL` to the
// integer representing the patch-level for that release.
//
// For example, for LTS release version "20300401.2", this would give us
// ABSL_LTS_RELEASE_VERSION == 20300401 && ABSL_LTS_RELEASE_PATCH_LEVEL == 2
//
// These symbols will not be defined in non-LTS code.
//
// Abseil recommends that clients live-at-head. Therefore, if you are using
// these symbols to assert a minimum version requirement, we recommend you do it
// as
//
// #if defined(ABSL_LTS_RELEASE_VERSION) && ABSL_LTS_RELEASE_VERSION < 20300401
// #error Project foo requires Abseil LTS version >= 20300401
// #endif
//
// The `defined(ABSL_LTS_RELEASE_VERSION)` part of the check excludes
// live-at-head clients from the minimum version assertion.
//
// See https://abseil.io/about/releases for more information on Abseil release
// management.
//
// LTS releases can be obtained from
// https://github.com/abseil/abseil-cpp/releases.
#undef ABSL_LTS_RELEASE_VERSION
#undef ABSL_LTS_RELEASE_PATCH_LEVEL

// Helper macro to convert a CPP variable to a string literal.
#define ABSL_INTERNAL_DO_TOKEN_STR(x) #x
#define ABSL_INTERNAL_TOKEN_STR(x) ABSL_INTERNAL_DO_TOKEN_STR(x)

// -----------------------------------------------------------------------------
// Abseil namespace annotations
// -----------------------------------------------------------------------------

// ABSL_NAMESPACE_BEGIN/ABSL_NAMESPACE_END
//
// An annotation placed at the beginning/end of each `namespace absl` scope.
// This is used to inject an inline namespace.
//
// The proper way to write Abseil code in the `absl` namespace is:
//
// namespace absl {
// ABSL_NAMESPACE_BEGIN
//
// void Foo();  // absl::Foo().
//
// ABSL_NAMESPACE_END
// }  // namespace absl
//
// Users of Abseil should not use these macros, because users of Abseil should
// not write `namespace absl {` in their own code for any reason.  (Abseil does
// not support forward declarations of its own types, nor does it support
// user-provided specialization of Abseil templates.  Code that violates these
// rules may be broken without warning.)
#if !defined(ABSL_OPTION_USE_INLINE_NAMESPACE) || \
    !defined(ABSL_OPTION_INLINE_NAMESPACE_NAME)
#error options.h is misconfigured.
#endif

// Check that ABSL_OPTION_INLINE_NAMESPACE_NAME is neither "head" nor ""
#if defined(__cplusplus) && ABSL_OPTION_USE_INLINE_NAMESPACE == 1

#define ABSL_INTERNAL_INLINE_NAMESPACE_STR \
  ABSL_INTERNAL_TOKEN_STR(ABSL_OPTION_INLINE_NAMESPACE_NAME)

static_assert(ABSL_INTERNAL_INLINE_NAMESPACE_STR[0] != '\0',
              "options.h misconfigured: ABSL_OPTION_INLINE_NAMESPACE_NAME must "
              "not be empty.");
static_assert(ABSL_INTERNAL_INLINE_NAMESPACE_STR[0] != 'h' ||
                  ABSL_INTERNAL_INLINE_NAMESPACE_STR[1] != 'e' ||
                  ABSL_INTERNAL_INLINE_NAMESPACE_STR[2] != 'a' ||
                  ABSL_INTERNAL_INLINE_NAMESPACE_STR[3] != 'd' ||
                  ABSL_INTERNAL_INLINE_NAMESPACE_STR[4] != '\0',
              "options.h misconfigured: ABSL_OPTION_INLINE_NAMESPACE_NAME must "
              "be changed to a new, unique identifier name.");

#endif

#if ABSL_OPTION_USE_INLINE_NAMESPACE == 0
#define ABSL_NAMESPACE_BEGIN
#define ABSL_NAMESPACE_END
#define ABSL_INTERNAL_C_SYMBOL(x) x
#elif ABSL_OPTION_USE_INLINE_NAMESPACE == 1
#define ABSL_NAMESPACE_BEGIN \
  inline namespace ABSL_OPTION_INLINE_NAMESPACE_NAME {
#define ABSL_NAMESPACE_END }
#define ABSL_INTERNAL_C_SYMBOL_HELPER_2(x, v) x##_##v
#define ABSL_INTERNAL_C_SYMBOL_HELPER_1(x, v) \
  ABSL_INTERNAL_C_SYMBOL_HELPER_2(x, v)
#define ABSL_INTERNAL_C_SYMBOL(x) \
  ABSL_INTERNAL_C_SYMBOL_HELPER_1(x, ABSL_OPTION_INLINE_NAMESPACE_NAME)
#else
#error options.h is misconfigured.
#endif

// -----------------------------------------------------------------------------
// Compiler Feature Checks
// -----------------------------------------------------------------------------

// ABSL_HAVE_BUILTIN()
//
// Checks whether the compiler supports a Clang Feature Checking Macro, and if
// so, checks whether it supports the provided builtin function "x" where x
// is one of the functions noted in
// https://clang.llvm.org/docs/LanguageExtensions.html
//
// Note: Use this macro to avoid an extra level of #ifdef __has_builtin check.
// http://releases.llvm.org/3.3/tools/clang/docs/LanguageExtensions.html
#ifdef __has_builtin
#define ABSL_HAVE_BUILTIN(x) __has_builtin(x)
#else
#define ABSL_HAVE_BUILTIN(x) 0
#endif

#if defined(__is_identifier)
#define ABSL_INTERNAL_HAS_KEYWORD(x) !(__is_identifier(x))
#else
#define ABSL_INTERNAL_HAS_KEYWORD(x) 0
#endif

#ifdef __has_feature
#define ABSL_HAVE_FEATURE(f) __has_feature(f)
#else
#define ABSL_HAVE_FEATURE(f) 0
#endif

// Portable check for GCC minimum version:
// https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
#if defined(__GNUC__) && defined(__GNUC_MINOR__)
#define ABSL_INTERNAL_HAVE_MIN_GNUC_VERSION(x, y) \
  (__GNUC__ > (x) || __GNUC__ == (x) && __GNUC_MINOR__ >= (y))
#else
#define ABSL_INTERNAL_HAVE_MIN_GNUC_VERSION(x, y) 0
#endif

#if defined(__clang__) && defined(__clang_major__) && defined(__clang_minor__)
#define ABSL_INTERNAL_HAVE_MIN_CLANG_VERSION(x, y) \
  (__clang_major__ > (x) || __clang_major__ == (x) && __clang_minor__ >= (y))
#else
#define ABSL_INTERNAL_HAVE_MIN_CLANG_VERSION(x, y) 0
#endif

// ABSL_HAVE_TLS is defined to 1 when __thread should be supported.
// We assume __thread is supported on Linux when compiled with Clang or compiled
// against libstdc++ with _GLIBCXX_HAVE_TLS defined.
#ifdef ABSL_HAVE_TLS
#error ABSL_HAVE_TLS cannot be directly set
#elif defined(__linux__) && (defined(__clang__) || defined(_GLIBCXX_HAVE_TLS))
#define ABSL_HAVE_TLS 1
#endif

// ABSL_HAVE_STD_IS_TRIVIALLY_DESTRUCTIBLE
//
// Checks whether `std::is_trivially_destructible<T>` is supported.
//
// Notes: All supported compilers using libc++ support this feature, as does
// gcc >= 4.8.1 using libstdc++, and Visual Studio.
#ifdef ABSL_HAVE_STD_IS_TRIVIALLY_DESTRUCTIBLE
#error ABSL_HAVE_STD_IS_TRIVIALLY_DESTRUCTIBLE cannot be directly set
#elif defined(_LIBCPP_VERSION) || defined(_MSC_VER) || \
    (!defined(__clang__) && defined(__GLIBCXX__) &&    \
     ABSL_INTERNAL_HAVE_MIN_GNUC_VERSION(4, 8))
#define ABSL_HAVE_STD_IS_TRIVIALLY_DESTRUCTIBLE 1
#endif

// ABSL_HAVE_STD_IS_TRIVIALLY_CONSTRUCTIBLE
//
// Checks whether `std::is_trivially_default_constructible<T>` and
// `std::is_trivially_copy_constructible<T>` are supported.

// ABSL_HAVE_STD_IS_TRIVIALLY_ASSIGNABLE
//
// Checks whether `std::is_trivially_copy_assignable<T>` is supported.

// Notes: Clang with libc++ supports these features, as does gcc >= 7.4 with
// libstdc++, or gcc >= 8.2 with libc++, and Visual Studio (but not NVCC).
#if defined(ABSL_HAVE_STD_IS_TRIVIALLY_CONSTRUCTIBLE)
#error ABSL_HAVE_STD_IS_TRIVIALLY_CONSTRUCTIBLE cannot be directly set
#elif defined(ABSL_HAVE_STD_IS_TRIVIALLY_ASSIGNABLE)
#error ABSL_HAVE_STD_IS_TRIVIALLY_ASSIGNABLE cannot directly set
#elif (defined(__clang__) && defined(_LIBCPP_VERSION)) ||                    \
    (!defined(__clang__) &&                                                  \
     ((ABSL_INTERNAL_HAVE_MIN_GNUC_VERSION(7, 4) && defined(__GLIBCXX__)) || \
      (ABSL_INTERNAL_HAVE_MIN_GNUC_VERSION(8, 2) &&                          \
       defined(_LIBCPP_VERSION)))) ||                                        \
    (defined(_MSC_VER) && !defined(__NVCC__))
#define ABSL_HAVE_STD_IS_TRIVIALLY_CONSTRUCTIBLE 1
#define ABSL_HAVE_STD_IS_TRIVIALLY_ASSIGNABLE 1
#endif

// ABSL_HAVE_SOURCE_LOCATION_CURRENT
//
// Indicates whether `absl::SourceLocation::current()` will return useful
// information in some contexts.
#ifndef ABSL_HAVE_SOURCE_LOCATION_CURRENT
#if ABSL_INTERNAL_HAS_KEYWORD(__builtin_LINE) && \
    ABSL_INTERNAL_HAS_KEYWORD(__builtin_FILE)
#define ABSL_HAVE_SOURCE_LOCATION_CURRENT 1
#elif ABSL_INTERNAL_HAVE_MIN_GNUC_VERSION(5, 0)
#define ABSL_HAVE_SOURCE_LOCATION_CURRENT 1
#endif
#endif

// ABSL_HAVE_THREAD_LOCAL
//
// Checks whether C++11's `thread_local` storage duration specifier is
// supported.
#ifdef ABSL_HAVE_THREAD_LOCAL
#error ABSL_HAVE_THREAD_LOCAL cannot be directly set
#elif defined(__APPLE__)
// Notes:
// * Xcode's clang did not support `thread_local` until version 8, and
//   even then not for all iOS < 9.0.
// * Xcode 9.3 started disallowing `thread_local` for 32-bit iOS simulator
//   targeting iOS 9.x.
// * Xcode 10 moves the deployment target check for iOS < 9.0 to link time
//   making ABSL_HAVE_FEATURE unreliable there.
//
#if ABSL_HAVE_FEATURE(cxx_thread_local) && \
    !(TARGET_OS_IPHONE && __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_9_0)
#define ABSL_HAVE_THREAD_LOCAL 1
#endif
#else  // !defined(__APPLE__)
#define ABSL_HAVE_THREAD_LOCAL 1
#endif

// There are platforms for which TLS should not be used even though the compiler
// makes it seem like it's supported (Android NDK < r12b for example).
// This is primarily because of linker problems and toolchain misconfiguration:
// Abseil does not intend to support this indefinitely. Currently, the newest
// toolchain that we intend to support that requires this behavior is the
// r11 NDK - allowing for a 5 year support window on that means this option
// is likely to be removed around June of 2021.
// TLS isn't supported until NDK r12b per
// https://developer.android.com/ndk/downloads/revision_history.html
// Since NDK r16, `__NDK_MAJOR__` and `__NDK_MINOR__` are defined in
// <android/ndk-version.h>. For NDK < r16, users should define these macros,
// e.g. `-D__NDK_MAJOR__=11 -D__NKD_MINOR__=0` for NDK r11.
#if defined(__ANDROID__) && defined(__clang__)
#if __has_include(<android/ndk-version.h>)
#include <android/ndk-version.h>
#endif  // __has_include(<android/ndk-version.h>)
#if defined(__ANDROID__) && defined(__clang__) && defined(__NDK_MAJOR__) && \
    defined(__NDK_MINOR__) &&                                               \
    ((__NDK_MAJOR__ < 12) || ((__NDK_MAJOR__ == 12) && (__NDK_MINOR__ < 1)))
#undef ABSL_HAVE_TLS
#undef ABSL_HAVE_THREAD_LOCAL
#endif
#endif  // defined(__ANDROID__) && defined(__clang__)

// ABSL_HAVE_INTRINSIC_INT128
//
// Checks whether the __int128 compiler extension for a 128-bit integral type is
// supported.
//
// Note: __SIZEOF_INT128__ is defined by Clang and GCC when __int128 is
// supported, but we avoid using it in certain cases:
// * On Clang:
//   * Building using Clang for Windows, where the Clang runtime library has
//     128-bit support only on LP64 architectures, but Windows is LLP64.
// * On Nvidia's nvcc:
//   * nvcc also defines __GNUC__ and __SIZEOF_INT128__, but not all versions
//     actually support __int128.
#ifdef ABSL_HAVE_INTRINSIC_INT128
#error ABSL_HAVE_INTRINSIC_INT128 cannot be directly set
#elif defined(__SIZEOF_INT128__)
#if (defined(__clang__) && !defined(_WIN32)) || \
    (defined(__CUDACC__) && __CUDACC_VER_MAJOR__ >= 9) ||                \
    (defined(__GNUC__) && !defined(__clang__) && !defined(__CUDACC__))
#define ABSL_HAVE_INTRINSIC_INT128 1
#elif defined(__CUDACC__)
// __CUDACC_VER__ is a full version number before CUDA 9, and is defined to a
// string explaining that it has been removed starting with CUDA 9. We use
// nested #ifs because there is no short-circuiting in the preprocessor.
// NOTE: `__CUDACC__` could be undefined while `__CUDACC_VER__` is defined.
#if __CUDACC_VER__ >= 70000
#define ABSL_HAVE_INTRINSIC_INT128 1
#endif  // __CUDACC_VER__ >= 70000
#endif  // defined(__CUDACC__)
#endif  // ABSL_HAVE_INTRINSIC_INT128

// ABSL_HAVE_EXCEPTIONS
//
// Checks whether the compiler both supports and enables exceptions. Many
// compilers support a "no exceptions" mode that disables exceptions.
//
// Generally, when ABSL_HAVE_EXCEPTIONS is not defined:
//
// * Code using `throw` and `try` may not compile.
// * The `noexcept` specifier will still compile and behave as normal.
// * The `noexcept` operator may still return `false`.
//
// For further details, consult the compiler's documentation.
#ifdef ABSL_HAVE_EXCEPTIONS
#error ABSL_HAVE_EXCEPTIONS cannot be directly set.
#elif ABSL_INTERNAL_HAVE_MIN_CLANG_VERSION(3, 6)
// Clang >= 3.6
#if ABSL_HAVE_FEATURE(cxx_exceptions)
#define ABSL_HAVE_EXCEPTIONS 1
#endif  // ABSL_HAVE_FEATURE(cxx_exceptions)
#elif defined(__clang__)
// Clang < 3.6
// http://releases.llvm.org/3.6.0/tools/clang/docs/ReleaseNotes.html#the-exceptions-macro
#if defined(__EXCEPTIONS) && ABSL_HAVE_FEATURE(cxx_exceptions)
#define ABSL_HAVE_EXCEPTIONS 1
#endif  // defined(__EXCEPTIONS) && ABSL_HAVE_FEATURE(cxx_exceptions)
// Handle remaining special cases and default to exceptions being supported.
#elif !(defined(__GNUC__) && (__GNUC__ < 5) && !defined(__EXCEPTIONS)) && \
    !(ABSL_INTERNAL_HAVE_MIN_GNUC_VERSION(5, 0) &&                        \
      !defined(__cpp_exceptions)) &&                                      \
    !(defined(_MSC_VER) && !defined(_CPPUNWIND))
#define ABSL_HAVE_EXCEPTIONS 1
#endif

// -----------------------------------------------------------------------------
// Platform Feature Checks
// -----------------------------------------------------------------------------

// Currently supported operating systems and associated preprocessor
// symbols:
//
//   Linux and Linux-derived           __linux__
//   Android                           __ANDROID__ (implies __linux__)
//   Linux (non-Android)               __linux__ && !__ANDROID__
//   Darwin (macOS and iOS)            __APPLE__
//   Akaros (http://akaros.org)        __ros__
//   Windows                           _WIN32
//   NaCL                              __native_client__
//   AsmJS                             __asmjs__
//   WebAssembly                       __wasm__
//   Fuchsia                           __Fuchsia__
//
// Note that since Android defines both __ANDROID__ and __linux__, one
// may probe for either Linux or Android by simply testing for __linux__.

// ABSL_HAVE_MMAP
//
// Checks whether the platform has an mmap(2) implementation as defined in
// POSIX.1-2001.
#ifdef ABSL_HAVE_MMAP
#error ABSL_HAVE_MMAP cannot be directly set
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || \
    defined(_AIX) || defined(__ros__) || defined(__native_client__) ||    \
    defined(__asmjs__) || defined(__wasm__) || defined(__Fuchsia__) ||    \
    defined(__sun) || defined(__ASYLO__) || defined(__myriad2__) ||       \
    defined(__HAIKU__)
#define ABSL_HAVE_MMAP 1
#endif

// ABSL_HAVE_PTHREAD_GETSCHEDPARAM
//
// Checks whether the platform implements the pthread_(get|set)schedparam(3)
// functions as defined in POSIX.1-2001.
#ifdef ABSL_HAVE_PTHREAD_GETSCHEDPARAM
#error ABSL_HAVE_PTHREAD_GETSCHEDPARAM cannot be directly set
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || \
    defined(_AIX) || defined(__ros__)
#define ABSL_HAVE_PTHREAD_GETSCHEDPARAM 1
#endif

// ABSL_HAVE_SCHED_GETCPU
//
// Checks whether sched_getcpu is available.
#ifdef ABSL_HAVE_SCHED_GETCPU
#error ABSL_HAVE_SCHED_GETCPU cannot be directly set
#elif defined(__linux__)
#define ABSL_HAVE_SCHED_GETCPU 1
#endif

// ABSL_HAVE_SCHED_YIELD
//
// Checks whether the platform implements sched_yield(2) as defined in
// POSIX.1-2001.
#ifdef ABSL_HAVE_SCHED_YIELD
#error ABSL_HAVE_SCHED_YIELD cannot be directly set
#elif defined(__linux__) || defined(__ros__) || defined(__native_client__)
#define ABSL_HAVE_SCHED_YIELD 1
#endif

// ABSL_HAVE_SEMAPHORE_H
//
// Checks whether the platform supports the <semaphore.h> header and sem_init(3)
// family of functions as standardized in POSIX.1-2001.
//
// Note: While Apple provides <semaphore.h> for both iOS and macOS, it is
// explicitly deprecated and will cause build failures if enabled for those
// platforms.  We side-step the issue by not defining it here for Apple
// platforms.
#ifdef ABSL_HAVE_SEMAPHORE_H
#error ABSL_HAVE_SEMAPHORE_H cannot be directly set
#elif defined(__linux__) || defined(__ros__)
#define ABSL_HAVE_SEMAPHORE_H 1
#endif

// ABSL_HAVE_ALARM
//
// Checks whether the platform supports the <signal.h> header and alarm(2)
// function as standardized in POSIX.1-2001.
#ifdef ABSL_HAVE_ALARM
#error ABSL_HAVE_ALARM cannot be directly set
#elif defined(__GOOGLE_GRTE_VERSION__)
// feature tests for Google's GRTE
#define ABSL_HAVE_ALARM 1
#elif defined(__GLIBC__)
// feature test for glibc
#define ABSL_HAVE_ALARM 1
#elif defined(_MSC_VER)
// feature tests for Microsoft's library
#elif defined(__MINGW32__)
// mingw32 doesn't provide alarm(2):
// https://osdn.net/projects/mingw/scm/git/mingw-org-wsl/blobs/5.2-trunk/mingwrt/include/unistd.h
// mingw-w64 provides a no-op implementation:
// https://sourceforge.net/p/mingw-w64/mingw-w64/ci/master/tree/mingw-w64-crt/misc/alarm.c
#elif defined(__EMSCRIPTEN__)
// emscripten doesn't support signals
#elif defined(__Fuchsia__)
// Signals don't exist on fuchsia.
#elif defined(__native_client__)
#else
// other standard libraries
#define ABSL_HAVE_ALARM 1
#endif

// ABSL_IS_LITTLE_ENDIAN
// ABSL_IS_BIG_ENDIAN
//
// Checks the endianness of the platform.
//
// Notes: uses the built in endian macros provided by GCC (since 4.6) and
// Clang (since 3.2); see
// https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html.
// Otherwise, if _WIN32, assume little endian. Otherwise, bail with an error.
#if defined(ABSL_IS_BIG_ENDIAN)
#error "ABSL_IS_BIG_ENDIAN cannot be directly set."
#endif
#if defined(ABSL_IS_LITTLE_ENDIAN)
#error "ABSL_IS_LITTLE_ENDIAN cannot be directly set."
#endif

#if (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
     __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define ABSL_IS_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ABSL_IS_BIG_ENDIAN 1
#elif defined(_WIN32)
#define ABSL_IS_LITTLE_ENDIAN 1
#else
#error "absl endian detection needs to be set up for your compiler"
#endif

// macOS < 10.13 and iOS < 11 don't let you use <any>, <optional>, or <variant>
// even though the headers exist and are publicly noted to work, because the
// libc++ shared library shipped on the system doesn't have the requisite
// exported symbols.  See https://github.com/abseil/abseil-cpp/issues/207 and
// https://developer.apple.com/documentation/xcode_release_notes/xcode_10_release_notes
//
// libc++ spells out the availability requirements in the file
// llvm-project/libcxx/include/__config via the #define
// _LIBCPP_AVAILABILITY_BAD_OPTIONAL_ACCESS.
//
// Unfortunately, Apple initially mis-stated the requirements as macOS < 10.14
// and iOS < 12 in the libc++ headers. This was corrected by
// https://github.com/llvm/llvm-project/commit/7fb40e1569dd66292b647f4501b85517e9247953
// which subsequently made it into the XCode 12.5 release. We need to match the
// old (incorrect) conditions when built with old XCode, but can use the
// corrected earlier versions with new XCode.
#if defined(__APPLE__) && defined(_LIBCPP_VERSION) &&               \
    ((_LIBCPP_VERSION >= 11000 && /* XCode 12.5 or later: */        \
      ((defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) &&   \
        __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 101300) ||  \
       (defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) &&  \
        __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ < 110000) || \
       (defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) &&   \
        __ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__ < 40000) ||   \
       (defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) &&      \
        __ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__ < 110000))) ||   \
     (_LIBCPP_VERSION < 11000 && /* Pre-XCode 12.5: */              \
      ((defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) &&   \
        __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 101400) ||  \
       (defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) &&  \
        __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ < 120000) || \
       (defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) &&   \
        __ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__ < 50000) ||   \
       (defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) &&      \
        __ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__ < 120000))))
#define ABSL_INTERNAL_APPLE_CXX17_TYPES_UNAVAILABLE 1
#else
#define ABSL_INTERNAL_APPLE_CXX17_TYPES_UNAVAILABLE 0
#endif

// ABSL_HAVE_STD_ANY
//
// Checks whether C++17 std::any is available by checking whether <any> exists.
#ifdef ABSL_HAVE_STD_ANY
#error "ABSL_HAVE_STD_ANY cannot be directly set."
#endif

#ifdef __has_include
#if __has_include(<any>) && defined(__cplusplus) && __cplusplus >= 201703L && \
    !ABSL_INTERNAL_APPLE_CXX17_TYPES_UNAVAILABLE
#define ABSL_HAVE_STD_ANY 1
#endif
#endif

// ABSL_HAVE_STD_OPTIONAL
//
// Checks whether C++17 std::optional is available.
#ifdef ABSL_HAVE_STD_OPTIONAL
#error "ABSL_HAVE_STD_OPTIONAL cannot be directly set."
#endif

#ifdef __has_include
#if __has_include(<optional>) && defined(__cplusplus) && \
    __cplusplus >= 201703L && !ABSL_INTERNAL_APPLE_CXX17_TYPES_UNAVAILABLE
#define ABSL_HAVE_STD_OPTIONAL 1
#endif
#endif

// ABSL_HAVE_STD_VARIANT
//
// Checks whether C++17 std::variant is available.
#ifdef ABSL_HAVE_STD_VARIANT
#error "ABSL_HAVE_STD_VARIANT cannot be directly set."
#endif

#ifdef __has_include
#if __has_include(<variant>) && defined(__cplusplus) && \
    __cplusplus >= 201703L && !ABSL_INTERNAL_APPLE_CXX17_TYPES_UNAVAILABLE
#define ABSL_HAVE_STD_VARIANT 1
#endif
#endif

// ABSL_HAVE_STD_STRING_VIEW
//
// Checks whether C++17 std::string_view is available.
#ifdef ABSL_HAVE_STD_STRING_VIEW
#error "ABSL_HAVE_STD_STRING_VIEW cannot be directly set."
#endif

#ifdef __has_include
#if __has_include(<string_view>) && defined(__cplusplus) && \
    __cplusplus >= 201703L
#define ABSL_HAVE_STD_STRING_VIEW 1
#endif
#endif

// For MSVC, `__has_include` is supported in VS 2017 15.3, which is later than
// the support for <optional>, <any>, <string_view>, <variant>. So we use
// _MSC_VER to check whether we have VS 2017 RTM (when <optional>, <any>,
// <string_view>, <variant> is implemented) or higher. Also, `__cplusplus` is
// not correctly set by MSVC, so we use `_MSVC_LANG` to check the language
// version.
// TODO(zhangxy): fix tests before enabling aliasing for `std::any`.
#if defined(_MSC_VER) && _MSC_VER >= 1910 &&         \
    ((defined(_MSVC_LANG) && _MSVC_LANG > 201402) || \
     (defined(__cplusplus) && __cplusplus > 201402))
// #define ABSL_HAVE_STD_ANY 1
#define ABSL_HAVE_STD_OPTIONAL 1
#define ABSL_HAVE_STD_VARIANT 1
#define ABSL_HAVE_STD_STRING_VIEW 1
#endif

// ABSL_USES_STD_ANY
//
// Indicates whether absl::any is an alias for std::any.
#if !defined(ABSL_OPTION_USE_STD_ANY)
#error options.h is misconfigured.
#elif ABSL_OPTION_USE_STD_ANY == 0 || \
    (ABSL_OPTION_USE_STD_ANY == 2 && !defined(ABSL_HAVE_STD_ANY))
#undef ABSL_USES_STD_ANY
#elif ABSL_OPTION_USE_STD_ANY == 1 || \
    (ABSL_OPTION_USE_STD_ANY == 2 && defined(ABSL_HAVE_STD_ANY))
#define ABSL_USES_STD_ANY 1
#else
#error options.h is misconfigured.
#endif

// ABSL_USES_STD_OPTIONAL
//
// Indicates whether absl::optional is an alias for std::optional.
#if !defined(ABSL_OPTION_USE_STD_OPTIONAL)
#error options.h is misconfigured.
#elif ABSL_OPTION_USE_STD_OPTIONAL == 0 || \
    (ABSL_OPTION_USE_STD_OPTIONAL == 2 && !defined(ABSL_HAVE_STD_OPTIONAL))
#undef ABSL_USES_STD_OPTIONAL
#elif ABSL_OPTION_USE_STD_OPTIONAL == 1 || \
    (ABSL_OPTION_USE_STD_OPTIONAL == 2 && defined(ABSL_HAVE_STD_OPTIONAL))
#define ABSL_USES_STD_OPTIONAL 1
#else
#error options.h is misconfigured.
#endif

// ABSL_USES_STD_VARIANT
//
// Indicates whether absl::variant is an alias for std::variant.
#if !defined(ABSL_OPTION_USE_STD_VARIANT)
#error options.h is misconfigured.
#elif ABSL_OPTION_USE_STD_VARIANT == 0 || \
    (ABSL_OPTION_USE_STD_VARIANT == 2 && !defined(ABSL_HAVE_STD_VARIANT))
#undef ABSL_USES_STD_VARIANT
#elif ABSL_OPTION_USE_STD_VARIANT == 1 || \
    (ABSL_OPTION_USE_STD_VARIANT == 2 && defined(ABSL_HAVE_STD_VARIANT))
#define ABSL_USES_STD_VARIANT 1
#else
#error options.h is misconfigured.
#endif

// ABSL_USES_STD_STRING_VIEW
//
// Indicates whether absl::string_view is an alias for std::string_view.
#if !defined(ABSL_OPTION_USE_STD_STRING_VIEW)
#error options.h is misconfigured.
#elif ABSL_OPTION_USE_STD_STRING_VIEW == 0 || \
    (ABSL_OPTION_USE_STD_STRING_VIEW == 2 &&  \
     !defined(ABSL_HAVE_STD_STRING_VIEW))
#undef ABSL_USES_STD_STRING_VIEW
#elif ABSL_OPTION_USE_STD_STRING_VIEW == 1 || \
    (ABSL_OPTION_USE_STD_STRING_VIEW == 2 &&  \
     defined(ABSL_HAVE_STD_STRING_VIEW))
#define ABSL_USES_STD_STRING_VIEW 1
#else
#error options.h is misconfigured.
#endif

// In debug mode, MSVC 2017's std::variant throws a EXCEPTION_ACCESS_VIOLATION
// SEH exception from emplace for variant<SomeStruct> when constructing the
// struct can throw. This defeats some of variant_test and
// variant_exception_safety_test.
#if defined(_MSC_VER) && _MSC_VER >= 1700 && defined(_DEBUG)
#define ABSL_INTERNAL_MSVC_2017_DBG_MODE
#endif

// ABSL_INTERNAL_MANGLED_NS
// ABSL_INTERNAL_MANGLED_BACKREFERENCE
//
// Internal macros for building up mangled names in our internal fork of CCTZ.
// This implementation detail is only needed and provided for the MSVC build.
//
// These macros both expand to string literals.  ABSL_INTERNAL_MANGLED_NS is
// the mangled spelling of the `absl` namespace, and
// ABSL_INTERNAL_MANGLED_BACKREFERENCE is a back-reference integer representing
// the proper count to skip past the CCTZ fork namespace names.  (This number
// is one larger when there is an inline namespace name to skip.)
#if defined(_MSC_VER)
#if ABSL_OPTION_USE_INLINE_NAMESPACE == 0
#define ABSL_INTERNAL_MANGLED_NS "absl"
#define ABSL_INTERNAL_MANGLED_BACKREFERENCE "5"
#else
#define ABSL_INTERNAL_MANGLED_NS \
  ABSL_INTERNAL_TOKEN_STR(ABSL_OPTION_INLINE_NAMESPACE_NAME) "@absl"
#define ABSL_INTERNAL_MANGLED_BACKREFERENCE "6"
#endif
#endif

#undef ABSL_INTERNAL_HAS_KEYWORD

// ABSL_DLL
//
// When building Abseil as a DLL, this macro expands to `__declspec(dllexport)`
// so we can annotate symbols appropriately as being exported. When used in
// headers consuming a DLL, this macro expands to `__declspec(dllimport)` so
// that consumers know the symbol is defined inside the DLL. In all other cases,
// the macro expands to nothing.
#if defined(_MSC_VER)
#if defined(ABSL_BUILD_DLL)
#define ABSL_DLL __declspec(dllexport)
#elif defined(ABSL_CONSUME_DLL)
#define ABSL_DLL __declspec(dllimport)
#else
#define ABSL_DLL
#endif
#else
#define ABSL_DLL
#endif  // defined(_MSC_VER)

// ABSL_HAVE_MEMORY_SANITIZER
//
// MemorySanitizer (MSan) is a detector of uninitialized reads. It consists of
// a compiler instrumentation module and a run-time library.
#ifdef ABSL_HAVE_MEMORY_SANITIZER
#error "ABSL_HAVE_MEMORY_SANITIZER cannot be directly set."
#elif !defined(__native_client__) && ABSL_HAVE_FEATURE(memory_sanitizer)
#define ABSL_HAVE_MEMORY_SANITIZER 1
#endif

// ABSL_HAVE_THREAD_SANITIZER
//
// ThreadSanitizer (TSan) is a fast data race detector.
#ifdef ABSL_HAVE_THREAD_SANITIZER
#error "ABSL_HAVE_THREAD_SANITIZER cannot be directly set."
#elif defined(__SANITIZE_THREAD__)
#define ABSL_HAVE_THREAD_SANITIZER 1
#elif ABSL_HAVE_FEATURE(thread_sanitizer)
#define ABSL_HAVE_THREAD_SANITIZER 1
#endif

// ABSL_HAVE_ADDRESS_SANITIZER
//
// AddressSanitizer (ASan) is a fast memory error detector.
#ifdef ABSL_HAVE_ADDRESS_SANITIZER
#error "ABSL_HAVE_ADDRESS_SANITIZER cannot be directly set."
#elif defined(__SANITIZE_ADDRESS__)
#define ABSL_HAVE_ADDRESS_SANITIZER 1
#elif ABSL_HAVE_FEATURE(address_sanitizer)
#define ABSL_HAVE_ADDRESS_SANITIZER 1
#endif

// ABSL_HAVE_HWADDRESS_SANITIZER
//
// Hardware-Assisted AddressSanitizer (or HWASAN) is even faster than asan
// memory error detector which can use CPU features like ARM TBI, Intel LAM or
// AMD UAI.
#ifdef ABSL_HAVE_HWADDRESS_SANITIZER
#error "ABSL_HAVE_HWADDRESS_SANITIZER cannot be directly set."
#elif defined(__SANITIZE_HWADDRESS__)
#define ABSL_HAVE_HWADDRESS_SANITIZER 1
#elif ABSL_HAVE_FEATURE(hwaddress_sanitizer)
#define ABSL_HAVE_HWADDRESS_SANITIZER 1
#endif

// ABSL_HAVE_LEAK_SANITIZER
//
// LeakSanitizer (or lsan) is a detector of memory leaks.
#ifdef ABSL_HAVE_LEAK_SANITIZER
#error "ABSL_HAVE_LEAK_SANITIZER cannot be directly set."
#elif ABSL_HAVE_FEATURE(leak_sanitizer)
#define ABSL_HAVE_LEAK_SANITIZER 1
#endif

// ABSL_HAVE_CLASS_TEMPLATE_ARGUMENT_DEDUCTION
//
// Class template argument deduction is a language feature added in C++17.
#ifdef ABSL_HAVE_CLASS_TEMPLATE_ARGUMENT_DEDUCTION
#error "ABSL_HAVE_CLASS_TEMPLATE_ARGUMENT_DEDUCTION cannot be directly set."
#elif defined(__cpp_deduction_guides)
#define ABSL_HAVE_CLASS_TEMPLATE_ARGUMENT_DEDUCTION 1
#endif

// `ABSL_INTERNAL_HAS_RTTI` determines whether abseil is being compiled with
// RTTI support.
#ifdef ABSL_INTERNAL_HAS_RTTI
#error ABSL_INTERNAL_HAS_RTTI cannot be directly set
#elif !defined(__GNUC__) || defined(__GXX_RTTI)
#define ABSL_INTERNAL_HAS_RTTI 1
#endif  // !defined(__GNUC__) || defined(__GXX_RTTI)

#endif  // ABSL_BASE_CONFIG_H_
