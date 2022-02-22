// Copyright Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef S2_BASE_PORT_H_
#define S2_BASE_PORT_H_

// This file contains things that are not used in third_party/absl but needed by
// - Platform specific requirement
//   - MSVC
// - Utility macros
// - Endianness
// - Hash
// - Global variables
// - Type alias
// - Predefined system/language macros
// - Predefined system/language functions
// - Performance optimization (alignment)
// - Obsolete

#include <inttypes.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "s2/base/integral_types.h"
#include "absl/base/config.h"
#include "absl/base/port.h"  // IWYU pragma: keep

#ifdef SWIG
%include "third_party/absl/base/port.h"
#endif

// -----------------------------------------------------------------------------
// MSVC Specific Requirements
// -----------------------------------------------------------------------------

#ifdef _MSC_VER /* if Visual C++ */

#include <intrin.h>
#include <process.h>  // _getpid()

// clang-format off
#include <winsock2.h>  // Must come before <windows.h>
#include <windows.h>
// clang-format on

#undef ERROR
#undef DELETE
#undef DIFFERENCE
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

// This compiler flag can be easily overlooked on MSVC.
// _CHAR_UNSIGNED gets set with the /J flag.
#ifndef _CHAR_UNSIGNED
#error chars must be unsigned!  Use the /J flag on the compiler command line.  // NOLINT
#endif

// Allow comparisons between signed and unsigned values.
//
// Lots of Google code uses this pattern:
//   for (int i = 0; i < container.size(); ++i)
// Since size() returns an unsigned value, this warning would trigger
// frequently.  Very few of these instances are actually bugs since containers
// rarely exceed MAX_INT items.  Unfortunately, there are bugs related to
// signed-unsigned comparisons that have been missed because we disable this
// warning.  For example:
//   const long stop_time = os::GetMilliseconds() + kWaitTimeoutMillis;
//   while (os::GetMilliseconds() <= stop_time) { ... }
#pragma warning(disable : 4018)  // level 3
#pragma warning(disable : 4267)  // level 3

// Don't warn about unused local variables.
//
// extension to silence particular instances of this warning.  There's no way
// to define ABSL_ATTRIBUTE_UNUSED to quiet particular instances of this warning
// in VC++, so we disable it globally.  Currently, there aren't many false
// positives, so perhaps we can address those in the future and re-enable these
// warnings, which sometimes catch real bugs.
#pragma warning(disable : 4101)  // level 3

// Allow initialization and assignment to a smaller type without warnings about
// possible loss of data.
//
// There is a distinct warning, 4267, that warns about size_t conversions to
// smaller types, but we don't currently disable that warning.
//
// Correct code can be written in such a way as to avoid false positives
// by making the conversion explicit, but Google code isn't usually that
// verbose.  There are too many false positives to address at this time.  Note
// that this warning triggers at levels 2, 3, and 4 depending on the specific
// type of conversion.  By disabling it, we not only silence minor narrowing
// conversions but also serious ones.
#pragma warning(disable : 4244)  // level 2, 3, and 4

// Allow silent truncation of double to float.
//
// Silencing this warning has caused us to miss some subtle bugs.
#pragma warning(disable : 4305)  // level 1

// Allow a constant to be assigned to a type that is too small.
//
// I don't know why we allow this at all.  I can't think of a case where this
// wouldn't be a bug, but enabling the warning breaks many builds today.
#pragma warning(disable : 4307)  // level 2

// Allow passing the this pointer to an initializer even though it refers
// to an uninitialized object.
//
// Some observer implementations rely on saving the this pointer.  Those are
// safe because the pointer is not dereferenced until after the object is fully
// constructed.  This could however, obscure other instances.  In the future, we
// should look into disabling this warning locally rather globally.
#pragma warning(disable : 4355)  // level 1 and 4

// Allow implicit coercion from an integral type to a bool.
//
// These could be avoided by making the code more explicit, but that's never
// been the style here, so there would be many false positives.  It's not
// obvious if a true positive would ever help to find an actual bug.
#pragma warning(disable : 4800)  // level 3

#endif  // _MSC_VER

// -----------------------------------------------------------------------------
// Endianness
// -----------------------------------------------------------------------------

// IS_LITTLE_ENDIAN, IS_BIG_ENDIAN
#if defined __linux__ || defined OS_ANDROID || defined(__ANDROID__)
// TODO(user): http://b/21460321; use one of OS_ANDROID or __ANDROID__.
// _BIG_ENDIAN
#include <endian.h>

#elif defined(__APPLE__)

// BIG_ENDIAN
#include <machine/endian.h>  // NOLINT(build/include)

/* Let's try and follow the Linux convention */
#define __BYTE_ORDER  BYTE_ORDER
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#define __BIG_ENDIAN BIG_ENDIAN

#endif

// defines __BYTE_ORDER
#ifdef _WIN32
#define __BYTE_ORDER __LITTLE_ENDIAN
#define IS_LITTLE_ENDIAN
#else // _WIN32

// define the macros IS_LITTLE_ENDIAN or IS_BIG_ENDIAN
// using the above endian definitions from endian.h if
// endian.h was included
#ifdef __BYTE_ORDER
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define IS_LITTLE_ENDIAN
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
#define IS_BIG_ENDIAN
#endif

#else  // __BYTE_ORDER

#if defined(__LITTLE_ENDIAN__)
#define IS_LITTLE_ENDIAN
#elif defined(__BIG_ENDIAN__)
#define IS_BIG_ENDIAN
#endif

#endif  // __BYTE_ORDER
#endif  // _WIN32

// byte swap functions (bswap_16, bswap_32, bswap_64).

// The following guarantees declaration of the byte swap functions
#ifdef _MSC_VER
#include <cstdlib>  // NOLINT(build/include)

#define bswap_16(x) _byteswap_ushort(x)
#define bswap_32(x) _byteswap_ulong(x)
#define bswap_64(x) _byteswap_uint64(x)

#elif defined(__APPLE__)
// Mac OS X / Darwin features
#include <libkern/OSByteOrder.h>

#define bswap_16(x) OSSwapInt16(x)
#define bswap_32(x) OSSwapInt32(x)
#define bswap_64(x) OSSwapInt64(x)

#elif defined(__GLIBC__) || defined(__BIONIC__) || defined(__ASYLO__) || \
    0
#include <byteswap.h>  // IWYU pragma: export

#else

static inline uint16 bswap_16(uint16 x) {
#ifdef __cplusplus
  return static_cast<uint16>(((x & 0xFF) << 8) | ((x & 0xFF00) >> 8));
#else
  return (uint16)(((x & 0xFF) << 8) | ((x & 0xFF00) >> 8));  // NOLINT
#endif  // __cplusplus
}
#define bswap_16(x) bswap_16(x)
static inline uint32 bswap_32(uint32 x) {
  return (((x & 0xFF) << 24) |
          ((x & 0xFF00) << 8) |
          ((x & 0xFF0000) >> 8) |
          ((x & 0xFF000000) >> 24));
}
#define bswap_32(x) bswap_32(x)
static inline uint64 bswap_64(uint64 x) {
  return (((x & (uint64)0xFF) << 56) | ((x & (uint64)0xFF00) << 40) |
          ((x & (uint64)0xFF0000) << 24) | ((x & (uint64)0xFF000000) << 8) |
          ((x & (uint64)0xFF00000000) >> 8) |
          ((x & (uint64)0xFF0000000000) >> 24) |
          ((x & (uint64)0xFF000000000000) >> 40) |
          ((x & (uint64)0xFF00000000000000) >> 56));
}
#define bswap_64(x) bswap_64(x)

#endif

// printf macros
// __STDC_FORMAT_MACROS must be defined before inttypes.h inclusion */
#if defined(__APPLE__)
/* From MacOSX's inttypes.h:
 * "C++ implementations should define these macros only when
 *  __STDC_FORMAT_MACROS is defined before <inttypes.h> is included." */
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif /* __STDC_FORMAT_MACROS */
#endif /* __APPLE__ */

// printf macros for size_t, in the style of inttypes.h
#if defined(_LP64) || defined(__APPLE__)
#define __PRIS_PREFIX "z"
#else
#define __PRIS_PREFIX
#endif

// Use these macros after a % in a printf format string
// to get correct 32/64 bit behavior, like this:
// size_t size = records.size();
// printf("%" PRIuS "\n", size);
#define PRIdS __PRIS_PREFIX "d"
#define PRIxS __PRIS_PREFIX "x"
#define PRIuS __PRIS_PREFIX "u"
#define PRIXS __PRIS_PREFIX "X"
#define PRIoS __PRIS_PREFIX "o"

// -----------------------------------------------------------------------------
// Performance Optimization
// -----------------------------------------------------------------------------

// Alignment

// Unaligned APIs

// Portable handling of unaligned loads, stores, and copies. These are simply
// constant-length memcpy calls.
//
// TODO(user): These APIs are forked in Abseil, see
// "third_party/absl/base/internal/unaligned_access.h".
//
// The unaligned API is C++ only.  The declarations use C++ features
// (namespaces, inline) which are absent or incompatible in C.
#if defined(__cplusplus)

inline uint16 UNALIGNED_LOAD16(const void *p) {
  uint16 t;
  memcpy(&t, p, sizeof t);
  return t;
}

inline uint32 UNALIGNED_LOAD32(const void *p) {
  uint32 t;
  memcpy(&t, p, sizeof t);
  return t;
}

inline uint64 UNALIGNED_LOAD64(const void *p) {
  uint64 t;
  memcpy(&t, p, sizeof t);
  return t;
}

inline void UNALIGNED_STORE16(void *p, uint16 v) { memcpy(p, &v, sizeof v); }

inline void UNALIGNED_STORE32(void *p, uint32 v) { memcpy(p, &v, sizeof v); }

inline void UNALIGNED_STORE64(void *p, uint64 v) { memcpy(p, &v, sizeof v); }

// The UNALIGNED_LOADW and UNALIGNED_STOREW macros load and store values
// of type uword_t.
#ifdef _LP64
#define UNALIGNED_LOADW(_p) UNALIGNED_LOAD64(_p)
#define UNALIGNED_STOREW(_p, _val) UNALIGNED_STORE64(_p, _val)
#else
#define UNALIGNED_LOADW(_p) UNALIGNED_LOAD32(_p)
#define UNALIGNED_STOREW(_p, _val) UNALIGNED_STORE32(_p, _val)
#endif

inline void UnalignedCopy16(const void *src, void *dst) {
  memcpy(dst, src, 2);
}

inline void UnalignedCopy32(const void *src, void *dst) {
  memcpy(dst, src, 4);
}

inline void UnalignedCopy64(const void *src, void *dst) {
  memcpy(dst, src, 8);
}

#endif  // defined(__cplusplus), end of unaligned API

#endif  // S2_BASE_PORT_H_
