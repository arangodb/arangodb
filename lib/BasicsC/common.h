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

#ifndef TRIAGENS_BASICS_C_COMMON_H
#define TRIAGENS_BASICS_C_COMMON_H 1

// -----------------------------------------------------------------------------
// --SECTION--                                             configuration options
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Configuration
/// @{
////////////////////////////////////////////////////////////////////////////////

#define TRI_WITHIN_COMMON 1
#include "BasicsC/operating-system.h"
#include "BasicsC/local-configuration.h"
#include "BasicsC/application-exit.h"
#undef TRI_WITHIN_COMMON

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    C header files
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Configuration
/// @{
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef TRI_HAVE_PROCESS_H
#include <process.h>
#endif

#ifdef TRI_HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef TRI_HAVE_STDBOOL_H
#include <stdbool.h>
#endif

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef TRI_HAVE_STRINGS_H
#include <strings.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#ifdef TRI_HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef TRI_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

// .............................................................................
// The problem we have for visual studio is that if we include WinSock2.h here
// it may conflict later in some other source file. The conflict arises when
// windows.h is included BEFORE WinSock2.h -- this is a visual studio issue. For
// now be VERY careful to ensure that if you need windows.h, then you include
// this file AFTER common.h.
// .............................................................................

#ifdef TRI_HAVE_WINSOCK2_H
#include <WinSock2.h>
typedef long suseconds_t;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            basic triAGENS headers
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Configuration
/// @{
////////////////////////////////////////////////////////////////////////////////

#define TRI_WITHIN_COMMON 1
#include "BasicsC/voc-errors.h"
#include "BasicsC/error.h"
#include "BasicsC/memory.h"
#include "BasicsC/mimetypes.h"
#include "BasicsC/structures.h"
#undef TRI_WITHIN_COMMON

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              basic compiler stuff
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Configuration
/// @{
////////////////////////////////////////////////////////////////////////////////

#define TRI_WITHIN_COMMON 1
#include "BasicsC/system-compiler.h"
#include "BasicsC/system-functions.h"
#undef TRI_WITHIN_COMMON

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 low level helpers
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Configuration
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief const cast for C
////////////////////////////////////////////////////////////////////////////////

static inline void* CONST_CAST (void const* ptr) {
  union { void* p; void const* c; } cnv;

  cnv.c = ptr;
  return cnv.p;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a wrapper for assert()
///
/// This wrapper maps TRI_ASSERT_MAINTAINER() to (void) 0 for non-maintainers. 
/// It maps TRI_ASSERT_MAINTAINER() to assert() when TRI_ENABLE_MAINTAINER_MODE 
/// is set.
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE

#define TRI_ASSERT_MAINTAINER(what) assert(what)

#else

#ifdef __cplusplus

#define TRI_ASSERT_MAINTAINER(what) (static_cast<void> (0))

#else

#define TRI_ASSERT_MAINTAINER(what) ((void) (0))

#endif

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
