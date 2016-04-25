////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_SYSTEM__COMPILER_H
#define ARANGODB_BASICS_SYSTEM__COMPILER_H 1

#ifndef TRI_WITHIN_COMMON
#error use <Basics/Common.h>
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief GNU compiler
////////////////////////////////////////////////////////////////////////////////

#ifdef __GNUC__
#define TRI_HAVE_GCC_UNUSED 1
#define TRI_HAVE_GCC_ATTRIBUTE 1
#define TRI_HAVE_GCC_BUILTIN 1
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief mark a value as unused
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_GCC_UNUSED
#define TRI_UNUSED __attribute__((unused))
#else
#define TRI_UNUSED /* unused */
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief warn if return is unused
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_GCC_ATTRIBUTE
#define TRI_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define TRI_WARN_UNUSED_RESULT /* unused */
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sizetint_t
////////////////////////////////////////////////////////////////////////////////

#if defined(TRI_OVERLOAD_FUNCS_SIZE_T)
#if TRI_SIZEOF_SIZE_T == 8
#define sizetint_t uint64_t
#else
#define sizetint_t uint32_t
#endif
#endif

#endif
