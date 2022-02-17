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

#ifndef IRESEARCH_ABSL_BASE_INTERNAL_PER_THREAD_TLS_H_
#define IRESEARCH_ABSL_BASE_INTERNAL_PER_THREAD_TLS_H_

// This header defines two macros:
//
// If the platform supports thread-local storage:
//
// * IRESEARCH_ABSL_PER_THREAD_TLS_KEYWORD is the C keyword needed to declare a
//   thread-local variable
// * IRESEARCH_ABSL_PER_THREAD_TLS is 1
//
// Otherwise:
//
// * IRESEARCH_ABSL_PER_THREAD_TLS_KEYWORD is empty
// * IRESEARCH_ABSL_PER_THREAD_TLS is 0
//
// Microsoft C supports thread-local storage.
// GCC supports it if the appropriate version of glibc is available,
// which the programmer can indicate by defining IRESEARCH_ABSL_HAVE_TLS

#include "absl/base/port.h"  // For IRESEARCH_ABSL_HAVE_TLS

#if defined(IRESEARCH_ABSL_PER_THREAD_TLS)
#error IRESEARCH_ABSL_PER_THREAD_TLS cannot be directly set
#elif defined(IRESEARCH_ABSL_PER_THREAD_TLS_KEYWORD)
#error IRESEARCH_ABSL_PER_THREAD_TLS_KEYWORD cannot be directly set
#elif defined(IRESEARCH_ABSL_HAVE_TLS)
#define IRESEARCH_ABSL_PER_THREAD_TLS_KEYWORD __thread
#define IRESEARCH_ABSL_PER_THREAD_TLS 1
#elif defined(_MSC_VER)
#define IRESEARCH_ABSL_PER_THREAD_TLS_KEYWORD __declspec(thread)
#define IRESEARCH_ABSL_PER_THREAD_TLS 1
#else
#define IRESEARCH_ABSL_PER_THREAD_TLS_KEYWORD
#define IRESEARCH_ABSL_PER_THREAD_TLS 0
#endif

#endif  // IRESEARCH_ABSL_BASE_INTERNAL_PER_THREAD_TLS_H_
