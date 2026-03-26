// Copyright 2020 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HIGHWAY_HWY_DETECT_COMPILER_ARCH_H_
#define HIGHWAY_HWY_DETECT_COMPILER_ARCH_H_

// Detects compiler and arch from predefined macros. Zero dependencies for
// inclusion by foreach_target.h.

// Add to #if conditions to prevent IDE from graying out code.
#if (defined __CDT_PARSER__) || (defined __INTELLISENSE__) || \
    (defined Q_CREATOR_RUN) || (defined __CLANGD__) ||        \
    (defined GROK_ELLIPSIS_BUILD)
#define HWY_IDE 1
#else
#define HWY_IDE 0
#endif

//------------------------------------------------------------------------------
// Compiler

// Actual MSVC, not clang-cl, which defines _MSC_VER but doesn't behave like
// MSVC in other aspects (e.g. HWY_DIAGNOSTICS).
#if defined(_MSC_VER) && !defined(__clang__)
#define HWY_COMPILER_MSVC _MSC_VER
#else
#define HWY_COMPILER_MSVC 0
#endif

#if defined(_MSC_VER) && defined(__clang__)
#define HWY_COMPILER_CLANGCL _MSC_VER
#else
#define HWY_COMPILER_CLANGCL 0
#endif

#ifdef __INTEL_COMPILER
#define HWY_COMPILER_ICC __INTEL_COMPILER
#else
#define HWY_COMPILER_ICC 0
#endif

#ifdef __INTEL_LLVM_COMPILER
#define HWY_COMPILER_ICX __INTEL_LLVM_COMPILER
#else
#define HWY_COMPILER_ICX 0
#endif

// HWY_COMPILER_GCC is a generic macro for all compilers implementing the GNU
// compiler extensions (eg. Clang, Intel...)
#ifdef __GNUC__
#define HWY_COMPILER_GCC (__GNUC__ * 100 + __GNUC_MINOR__)
#else
#define HWY_COMPILER_GCC 0
#endif

// Clang or clang-cl, not GCC.
#ifdef __clang__
// In case of Apple LLVM (whose version number is unrelated to that of LLVM) or
// an invalid version number, deduce it from the presence of warnings.
// Originally based on
// https://github.com/simd-everywhere/simde/blob/47d6e603de9d04ee05cdfbc57cf282a02be1bf2a/simde/simde-detect-clang.h#L59.
// Please send updates below to them as well, thanks!
#if defined(__apple_build_version__) || __clang_major__ >= 999
#if __has_attribute(nouwtable)  // no new warnings in 16.0
#define HWY_COMPILER_CLANG 1600
#elif __has_warning("-Warray-parameter")
#define HWY_COMPILER_CLANG 1500
#elif __has_warning("-Wbitwise-instead-of-logical")
#define HWY_COMPILER_CLANG 1400
#elif __has_warning("-Wreserved-identifier")
#define HWY_COMPILER_CLANG 1300
#elif __has_warning("-Wformat-insufficient-args")
#define HWY_COMPILER_CLANG 1200
#elif __has_warning("-Wimplicit-const-int-float-conversion")
#define HWY_COMPILER_CLANG 1100
#elif __has_warning("-Wmisleading-indentation")
#define HWY_COMPILER_CLANG 1000
#elif defined(__FILE_NAME__)
#define HWY_COMPILER_CLANG 900
#elif __has_warning("-Wextra-semi-stmt") || \
    __has_builtin(__builtin_rotateleft32)
#define HWY_COMPILER_CLANG 800
// For reasons unknown, XCode 10.3 (Apple LLVM version 10.0.1) is apparently
// based on Clang 7, but does not support the warning we test.
// See https://en.wikipedia.org/wiki/Xcode#Toolchain_versions and
// https://trac.macports.org/wiki/XcodeVersionInfo.
#elif __has_warning("-Wc++98-compat-extra-semi") || \
    (defined(__apple_build_version__) && __apple_build_version__ >= 10010000)
#define HWY_COMPILER_CLANG 700
#else  // Anything older than 7.0 is not recommended for Highway.
#define HWY_COMPILER_CLANG 600
#endif  // __has_warning chain
#define HWY_COMPILER3_CLANG (HWY_COMPILER_CLANG * 100)
#else  // use normal version
#define HWY_COMPILER_CLANG (__clang_major__ * 100 + __clang_minor__)
#define HWY_COMPILER3_CLANG \
  (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#endif
#else  // Not clang
#define HWY_COMPILER_CLANG 0
#define HWY_COMPILER3_CLANG 0
#endif

#if HWY_COMPILER_GCC && !HWY_COMPILER_CLANG
#define HWY_COMPILER_GCC_ACTUAL HWY_COMPILER_GCC
#else
#define HWY_COMPILER_GCC_ACTUAL 0
#endif

// More than one may be nonzero, but we want at least one.
#if 0 == (HWY_COMPILER_MSVC + HWY_COMPILER_CLANGCL + HWY_COMPILER_ICC + \
          HWY_COMPILER_GCC + HWY_COMPILER_CLANG)
#error "Unsupported compiler"
#endif

// We should only detect one of these (only clang/clangcl overlap)
#if 1 <                                                                     \
    (!!HWY_COMPILER_MSVC + !!HWY_COMPILER_ICC + !!HWY_COMPILER_GCC_ACTUAL + \
     !!(HWY_COMPILER_CLANGCL | HWY_COMPILER_CLANG))
#error "Detected multiple compilers"
#endif

#ifdef __has_builtin
#define HWY_HAS_BUILTIN(name) __has_builtin(name)
#else
#define HWY_HAS_BUILTIN(name) 0
#endif

#ifdef __has_attribute
#define HWY_HAS_ATTRIBUTE(name) __has_attribute(name)
#else
#define HWY_HAS_ATTRIBUTE(name) 0
#endif

#ifdef __has_cpp_attribute
#define HWY_HAS_CPP_ATTRIBUTE(name) __has_cpp_attribute(name)
#else
#define HWY_HAS_CPP_ATTRIBUTE(name) 0
#endif

#ifdef __has_feature
#define HWY_HAS_FEATURE(name) __has_feature(name)
#else
#define HWY_HAS_FEATURE(name) 0
#endif

//------------------------------------------------------------------------------
// Architecture

#if defined(__i386__) || defined(_M_IX86)
#define HWY_ARCH_X86_32 1
#else
#define HWY_ARCH_X86_32 0
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define HWY_ARCH_X86_64 1
#else
#define HWY_ARCH_X86_64 0
#endif

#if HWY_ARCH_X86_32 && HWY_ARCH_X86_64
#error "Cannot have both x86-32 and x86-64"
#endif

#if HWY_ARCH_X86_32 || HWY_ARCH_X86_64
#define HWY_ARCH_X86 1
#else
#define HWY_ARCH_X86 0
#endif

#if defined(__powerpc64__) || defined(_M_PPC) || defined(__powerpc__)
#define HWY_ARCH_PPC 1
#else
#define HWY_ARCH_PPC 0
#endif

// aarch32 is currently not supported; please raise an issue if you want it.
#if defined(__ARM_ARCH_ISA_A64) || defined(__aarch64__) || defined(_M_ARM64)
#define HWY_ARCH_ARM_A64 1
#else
#define HWY_ARCH_ARM_A64 0
#endif

#if (defined(__ARM_ARCH) && __ARM_ARCH == 7) || (defined(_M_ARM) && _M_ARM == 7)
#define HWY_ARCH_ARM_V7 1
#else
#define HWY_ARCH_ARM_V7 0
#endif

#if HWY_ARCH_ARM_A64 && HWY_ARCH_ARM_V7
#error "Cannot have both A64 and V7"
#endif

// Any *supported* version of Arm, i.e. 7 or later
#if HWY_ARCH_ARM_A64 || HWY_ARCH_ARM_V7
#define HWY_ARCH_ARM 1
#else
#define HWY_ARCH_ARM 0
#endif

// Older than Armv7 (e.g. armel aka Armv5) => we do not support SIMD.
#if (defined(__arm__) || defined(_M_ARM)) && !HWY_ARCH_ARM
#define HWY_ARCH_ARM_OLD 1
#else
#define HWY_ARCH_ARM_OLD 0
#endif

#if defined(__EMSCRIPTEN__) || defined(__wasm__) || defined(__WASM__)
#define HWY_ARCH_WASM 1
#else
#define HWY_ARCH_WASM 0
#endif

#ifdef __riscv
#define HWY_ARCH_RVV 1
#else
#define HWY_ARCH_RVV 0
#endif

// It is an error to detect multiple architectures at the same time, but OK to
// detect none of the above.
#if (HWY_ARCH_X86 + HWY_ARCH_PPC + HWY_ARCH_ARM + HWY_ARCH_ARM_OLD + \
     HWY_ARCH_WASM + HWY_ARCH_RVV) > 1
#error "Must not detect more than one architecture"
#endif

#if defined(_WIN32) || defined(_WIN64)
#define HWY_OS_WIN 1
#else
#define HWY_OS_WIN 0
#endif

#if defined(linux) || defined(__linux__)
#define HWY_OS_LINUX 1
#else
#define HWY_OS_LINUX 0
#endif

//------------------------------------------------------------------------------
// Endianness

#if HWY_COMPILER_MSVC
#if HWY_ARCH_PPC && defined(_XBOX_VER) && _XBOX_VER >= 200
// XBox 360 is big-endian
#define HWY_IS_LITTLE_ENDIAN 0
#define HWY_IS_BIG_ENDIAN 1
#else
// All other targets supported by MSVC are little-endian
#define HWY_IS_LITTLE_ENDIAN 1
#define HWY_IS_BIG_ENDIAN 0
#endif  // HWY_ARCH_PPC && defined(_XBOX_VER) && _XBOX_VER >= 200
#elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define HWY_IS_LITTLE_ENDIAN 1
#define HWY_IS_BIG_ENDIAN 0
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define HWY_IS_LITTLE_ENDIAN 0
#define HWY_IS_BIG_ENDIAN 1
#else
#error "Unable to detect endianness or unsupported byte order"
#endif

#if (HWY_IS_LITTLE_ENDIAN + HWY_IS_BIG_ENDIAN) != 1
#error "Must only detect one byte order"
#endif

#endif  // HIGHWAY_HWY_DETECT_COMPILER_ARCH_H_
