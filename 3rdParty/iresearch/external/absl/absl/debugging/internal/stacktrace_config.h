/*
 * Copyright 2017 The Abseil Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

 * Defines IRESEARCH_ABSL_STACKTRACE_INL_HEADER to the *-inl.h containing
 * actual unwinder implementation.
 * This header is "private" to stacktrace.cc.
 * DO NOT include it into any other files.
*/
#ifndef IRESEARCH_ABSL_DEBUGGING_INTERNAL_STACKTRACE_CONFIG_H_
#define IRESEARCH_ABSL_DEBUGGING_INTERNAL_STACKTRACE_CONFIG_H_

#if defined(IRESEARCH_ABSL_STACKTRACE_INL_HEADER)
#error IRESEARCH_ABSL_STACKTRACE_INL_HEADER cannot be directly set

#elif defined(_WIN32)
#define IRESEARCH_ABSL_STACKTRACE_INL_HEADER \
    "absl/debugging/internal/stacktrace_win32-inl.inc"

#elif defined(__APPLE__)
// Thread local support required for UnwindImpl.
// Notes:
// * Xcode's clang did not support `thread_local` until version 8, and
//   even then not for all iOS < 9.0.
// * Xcode 9.3 started disallowing `thread_local` for 32-bit iOS simulator
//   targeting iOS 9.x.
// * Xcode 10 moves the deployment target check for iOS < 9.0 to link time
//   making __has_feature unreliable there.
//
// Otherwise, `__has_feature` is only supported by Clang so it has be inside
// `defined(__APPLE__)` check.
#if __has_feature(cxx_thread_local) && \
    !(TARGET_OS_IPHONE && __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_9_0)
#define IRESEARCH_ABSL_STACKTRACE_INL_HEADER \
  "absl/debugging/internal/stacktrace_generic-inl.inc"
#endif

#elif defined(__linux__) && !defined(__ANDROID__)

#if defined(NO_FRAME_POINTER) && \
    (defined(__i386__) || defined(__x86_64__) || defined(__aarch64__))
// Note: The libunwind-based implementation is not available to open-source
// users.
#define IRESEARCH_ABSL_STACKTRACE_INL_HEADER \
  "absl/debugging/internal/stacktrace_libunwind-inl.inc"
#define STACKTRACE_USES_LIBUNWIND 1
#elif defined(NO_FRAME_POINTER) && defined(__has_include)
#if __has_include(<execinfo.h>)
// Note: When using glibc this may require -funwind-tables to function properly.
#define IRESEARCH_ABSL_STACKTRACE_INL_HEADER \
  "absl/debugging/internal/stacktrace_generic-inl.inc"
#endif
#elif defined(__i386__) || defined(__x86_64__)
#define IRESEARCH_ABSL_STACKTRACE_INL_HEADER \
  "absl/debugging/internal/stacktrace_x86-inl.inc"
#elif defined(__ppc__) || defined(__PPC__)
#define IRESEARCH_ABSL_STACKTRACE_INL_HEADER \
  "absl/debugging/internal/stacktrace_powerpc-inl.inc"
#elif defined(__aarch64__)
#define IRESEARCH_ABSL_STACKTRACE_INL_HEADER \
  "absl/debugging/internal/stacktrace_aarch64-inl.inc"
#elif defined(__has_include)
#if __has_include(<execinfo.h>)
// Note: When using glibc this may require -funwind-tables to function properly.
#define IRESEARCH_ABSL_STACKTRACE_INL_HEADER \
  "absl/debugging/internal/stacktrace_generic-inl.inc"
#endif
#endif

#endif

// Fallback to the empty implementation.
#if !defined(IRESEARCH_ABSL_STACKTRACE_INL_HEADER)
#define IRESEARCH_ABSL_STACKTRACE_INL_HEADER \
  "absl/debugging/internal/stacktrace_unimplemented-inl.inc"
#endif

#endif  // IRESEARCH_ABSL_DEBUGGING_INTERNAL_STACKTRACE_CONFIG_H_
