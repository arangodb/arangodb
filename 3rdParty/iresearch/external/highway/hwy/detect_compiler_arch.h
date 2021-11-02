// Copyright 2020 Google LLC
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
    (defined Q_CREATOR_RUN) || (defined(__CLANGD__))
#define HWY_IDE 1
#else
#define HWY_IDE 0
#endif

//------------------------------------------------------------------------------
// Compiler

// clang-cl defines _MSC_VER but doesn't behave like MSVC in other aspects like
// used in HWY_DIAGNOSTICS(). We include a check that we are not clang for that
// purpose.
#if defined(_MSC_VER) && !defined(__clang__)
#define HWY_COMPILER_MSVC _MSC_VER
#else
#define HWY_COMPILER_MSVC 0
#endif

#ifdef __INTEL_COMPILER
#define HWY_COMPILER_ICC __INTEL_COMPILER
#else
#define HWY_COMPILER_ICC 0
#endif

#ifdef __GNUC__
#define HWY_COMPILER_GCC (__GNUC__ * 100 + __GNUC_MINOR__)
#else
#define HWY_COMPILER_GCC 0
#endif

// Clang can masquerade as MSVC/GCC, in which case both are set.
#ifdef __clang__
#ifdef __APPLE__
// Apple LLVM version is unrelated to the actual Clang version, which we need
// for enabling workarounds. Use the presence of warning flags to deduce it.
// Adapted from https://github.com/simd-everywhere/simde/ simde-detect-clang.h.
#if __has_warning("-Wformat-insufficient-args")
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
#elif __has_warning("-Wc++98-compat-extra-semi")
#define HWY_COMPILER_CLANG 700
#else  // Anything older than 7.0 is not recommended for Highway.
#define HWY_COMPILER_CLANG 600
#endif  // __has_warning chain
#else   // Non-Apple: normal version
#define HWY_COMPILER_CLANG (__clang_major__ * 100 + __clang_minor__)
#endif
#else  // Not clang
#define HWY_COMPILER_CLANG 0
#endif

// More than one may be nonzero, but we want at least one.
#if !HWY_COMPILER_MSVC && !HWY_COMPILER_ICC && !HWY_COMPILER_GCC && \
    !HWY_COMPILER_CLANG
#error "Unsupported compiler"
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

//------------------------------------------------------------------------------
// Architecture

#if defined(HWY_EMULATE_SVE)

#define HWY_ARCH_X86_32 0
#define HWY_ARCH_X86_64 0
#define HWY_ARCH_X86 0
#define HWY_ARCH_PPC 0
#define HWY_ARCH_ARM_A64 1
#define HWY_ARCH_ARM_V7 0
#define HWY_ARCH_ARM 1
#define HWY_ARCH_WASM 0
#define HWY_ARCH_RVV 0

#else

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

#if defined(__powerpc64__) || defined(_M_PPC)
#define HWY_ARCH_PPC 1
#else
#define HWY_ARCH_PPC 0
#endif

#if defined(__ARM_ARCH_ISA_A64) || defined(__aarch64__) || defined(_M_ARM64)
#define HWY_ARCH_ARM_A64 1
#else
#define HWY_ARCH_ARM_A64 0
#endif

#if defined(__arm__) || defined(_M_ARM)
#define HWY_ARCH_ARM_V7 1
#else
#define HWY_ARCH_ARM_V7 0
#endif

#if HWY_ARCH_ARM_A64 && HWY_ARCH_ARM_V7
#error "Cannot have both A64 and V7"
#endif

#if HWY_ARCH_ARM_A64 || HWY_ARCH_ARM_V7
#define HWY_ARCH_ARM 1
#else
#define HWY_ARCH_ARM 0
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

#endif // defined(HWY_EMULATE_SVE)

// It is an error to detect multiple architectures at the same time, but OK to
// detect none of the above.
#if (HWY_ARCH_X86 + HWY_ARCH_PPC + HWY_ARCH_ARM + HWY_ARCH_WASM + \
     HWY_ARCH_RVV) > 1
#error "Must not detect more than one architecture"
#endif

#endif  // HIGHWAY_HWY_DETECT_COMPILER_ARCH_H_
