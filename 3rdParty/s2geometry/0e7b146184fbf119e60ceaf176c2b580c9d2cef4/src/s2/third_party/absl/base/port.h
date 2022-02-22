// Copyright 2017 The Abseil Authors.
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
//
// This files is a forwarding header for other headers containing various
// portability macros and functions.
// This file is used for both C and C++!
// It also contains obsolete things that are pending cleanup but need to stay
// in Abseil for now.
//
// NOTE FOR GOOGLERS:
//
// IWYU pragma: private, include "base/port.h"

#ifndef S2_THIRD_PARTY_ABSL_BASE_PORT_H_
#define S2_THIRD_PARTY_ABSL_BASE_PORT_H_

#include "s2/third_party/absl/base/attributes.h"
#include "s2/third_party/absl/base/config.h"
#include "s2/third_party/absl/base/optimization.h"

#ifdef SWIG
%include "third_party/absl/base/attributes.h"
#endif

// -----------------------------------------------------------------------------
// Obsolete (to be removed)
// -----------------------------------------------------------------------------

// HAS_GLOBAL_STRING
// Some platforms have a ::string class that is different from ::std::string
// (although the interface is the same, of course).  On other platforms,
// ::string is the same as ::std::string.
#if defined(__cplusplus) && !defined(SWIG)
#include <string>
#ifndef HAS_GLOBAL_STRING
using std::basic_string;
using std::string;
// TODO(user): using std::wstring?
#endif  // HAS_GLOBAL_STRING
#endif  // SWIG, __cplusplus

// NOTE: These live in Abseil purely as a short-term layering workaround to
// resolve a dependency chain between util/hash/hash, absl/strings, and //base:
// in order for //base to depend on absl/strings, the includes of hash need
// to be in absl, not //base.  string_view defines hashes.
//
// -----------------------------------------------------------------------------
// HASH_NAMESPACE, HASH_NAMESPACE_DECLARATION_START/END
// -----------------------------------------------------------------------------

// Define the namespace for pre-C++11 functors for hash_map and hash_set.
// This is not the namespace for C++11 functors (that namespace is "std").
//
// We used to require that the build tool or Makefile provide this definition.
// Now we usually get it from testing target macros. If the testing target
// macros are different from an external definition, you will get a build
// error.
//
// TODO(user): always get HASH_NAMESPACE from testing target macros.

#if defined(__GNUC__) && defined(GOOGLE_GLIBCXX_VERSION)
// Crosstool v17 or later.
#define HASH_NAMESPACE __gnu_cxx
#elif defined(_MSC_VER)
// MSVC.
// http://msdn.microsoft.com/en-us/library/6x7w9f6z(v=vs.100).aspx
#define HASH_NAMESPACE stdext
#elif defined(__APPLE__)
// Xcode.
#define HASH_NAMESPACE __gnu_cxx
#elif defined(__GNUC__)
// Some other version of gcc.
#define HASH_NAMESPACE __gnu_cxx
#else
// HASH_NAMESPACE defined externally.
// TODO(user): make this an error. Do not use external value of
// HASH_NAMESPACE.
#endif

#ifndef HASH_NAMESPACE
// TODO(user): try to delete this.
// I think gcc 2.95.3 was the last toolchain to use this.
#define HASH_NAMESPACE_DECLARATION_START
#define HASH_NAMESPACE_DECLARATION_END
#else
#define HASH_NAMESPACE_DECLARATION_START namespace HASH_NAMESPACE {
#define HASH_NAMESPACE_DECLARATION_END }
#endif

#endif  // S2_THIRD_PARTY_ABSL_BASE_PORT_H_
