////////////////////////////////////////////////////////////////////////////////
/// @brief High-Performance Database Framework made by triagens
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_C_SYSTEM_COMPILER_H
#define TRIAGENS_BASICS_C_SYSTEM_COMPILER_H 1

#ifndef TRI_WITHIN_COMMON
#error use <BasicsC/common.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                    public defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup OperatingSystem
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief warn if return is unused
////////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_GCC_ATTRIBUTE
#define WARN_UNUSED __attribute__ ((warn_unused_result))
#else
#define WARN_UNUSED /* unused */
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief expect false
///
/// @param
///    a        the value
////////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_GCC_BUILTIN
#define EF(a) __builtin_expect(a, false)
#else
#define EF(a) a
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief expect true
///
/// @param
///    a        the value
////////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_GCC_BUILTIN
#define ET(a) __builtin_expect(a, true)
#else
#define ET(a) a
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief prefetch read
///
/// @param
///    a        the value to prefetch
////////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_GCC_BUILTIN
#define PR(a) __builtin_prefetch(a, 0, 0)
#else
#define PR(a) /* prefetch read */
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief prefetch write
///
/// @param
///    a        the value to prefetch
////////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_GCC_BUILTIN
#define PW(a) __builtin_prefetch(a, 1, 0)
#else
#define PW(a) /* prefetch write */
#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
