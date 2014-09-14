////////////////////////////////////////////////////////////////////////////////
/// @brief High-Performance Database Framework made by triagens
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_COMMON_H
#define ARANGODB_BASICS_COMMON_H 1

// -----------------------------------------------------------------------------
// --SECTION--                                             configuration options
// -----------------------------------------------------------------------------

#define TRI_WITHIN_COMMON 1
#include "Basics/operating-system.h"
#ifdef _WIN32
#include "Basics/local-configuration-win.h"
#else
#include "Basics/local-configuration.h"
#endif
#include "Basics/application-exit.h"

#include "build.h"

#ifdef _DEBUG
#define TRI_VERSION_FULL TRI_VERSION " [" TRI_PLATFORM "-DEBUG]"
#else
#define TRI_VERSION_FULL TRI_VERSION " [" TRI_PLATFORM "]"
#endif

#undef TRI_WITHIN_COMMON

// -----------------------------------------------------------------------------
// --SECTION--                                                    C header files
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                            basic triAGENS headers
// -----------------------------------------------------------------------------

#define TRI_WITHIN_COMMON 1
#include "Basics/voc-errors.h"
#include "Basics/error.h"
#include "Basics/debugging.h"
#include "Basics/memory.h"
#include "Basics/mimetypes.h"
#include "Basics/structures.h"
#undef TRI_WITHIN_COMMON

// -----------------------------------------------------------------------------
// --SECTION--                                              basic compiler stuff
// -----------------------------------------------------------------------------

#define TRI_WITHIN_COMMON 1
#include "Basics/system-compiler.h"
#include "Basics/system-functions.h"
#undef TRI_WITHIN_COMMON

// -----------------------------------------------------------------------------
// --SECTION--           C++ header files that are always present on all systems
// -----------------------------------------------------------------------------

#include <algorithm>
#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <fstream>
#include <stack>
#include <string>
#include <vector>

// C++11!!
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <atomic>

// -----------------------------------------------------------------------------
// --SECTION--                                                 low level helpers
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief const cast for C
////////////////////////////////////////////////////////////////////////////////

static inline void* CONST_CAST (void const* ptr) {
  union { void* p; void const* c; } cnv;

  cnv.c = ptr;
  return cnv.p;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief incrementing a uint64_t modulo a number with wraparound
////////////////////////////////////////////////////////////////////////////////

static inline uint64_t TRI_IncModU64 (uint64_t i, uint64_t len) {
  // Note that the dummy variable gives the compiler a (good) chance to
  // use a conditional move instruction instead of a branch. This actually
  // works on modern gcc.
  uint64_t dummy;
  dummy = (++i) - len;
  return i < len ? i : dummy;
}

static inline uint64_t TRI_DecModU64 (uint64_t i, uint64_t len) {
  if ((i--) != 0) {
    return i;
  }
  return len-1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fake spinlocks
/// spin locks seem to have issues when used under Valgrind
/// we thus mimic spinlocks using ordinary mutexes when in maintainer mode
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
#define TRI_FAKE_SPIN_LOCKS 1
#else
#undef TRI_FAKE_SPIN_LOCKS
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief backtrace functionality
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE

#if HAVE_BACKTRACE

#include <execinfo.h>

#define TRI_USE_DEMANGLING
#include <cxxabi.h>

#endif
static inline void _backtrace (void) {
#if HAVE_BACKTRACE
  void* stack_frames[50];
  size_t size, i;
  char** strings;

  size = backtrace(stack_frames, sizeof(stack_frames) / sizeof(void*));
  strings = backtrace_symbols(stack_frames, size);
  for (i = 0; i < size; i++) {
    if (strings != NULL) {
#ifdef TRI_USE_DEMANGLING
      char *mangled_name = nullptr, *offset_begin = nullptr, *offset_end = nullptr;

      // find parantheses and +address offset surrounding mangled name
      for (char *p = strings[i]; *p; ++p) {
        if (*p == '(') {
          mangled_name = p; 
        }
        else if (*p == '+') {
          offset_begin = p;
        }
        else if (*p == ')') {
          offset_end = p;
          break;
        }
      }

      // if the line could be processed, attempt to demangle the symbol
      if (mangled_name && offset_begin && offset_end && 
          mangled_name < offset_begin) {
        *mangled_name++ = '\0';
        *offset_begin++ = '\0';
        *offset_end++ = '\0';
        int status = 0;
        char * demangled_name = abi::__cxa_demangle(mangled_name, 0, 0, &status);
        if (demangled_name != nullptr) {
          if (status == 0) {
            fprintf(stderr, "%s() [%p] %s\n", strings[i], stack_frames[i], demangled_name);
          }
          else {
            fprintf(stderr, "%s\n", strings[i]);
          }
          TRI_SystemFree(demangled_name);
        }
      }
      else
#endif
      {
        fprintf(stderr, "%s\n", strings[i]);
      }
    }
    else {
      fprintf(stderr, "[%p]\n", stack_frames[i]);
    }
  }
  if (strings != NULL) {
    TRI_SystemFree(strings);  
  }
#endif
}

static inline void _getBacktrace (std::string& btstr) {
#if HAVE_BACKTRACE
  void* stack_frames[50];
  size_t size, i;
  char** strings;

  size = backtrace(stack_frames, sizeof(stack_frames) / sizeof(void*));
  strings = backtrace_symbols(stack_frames, size);
  for (i = 0; i < size; i++) {
    std::stringstream ss;
    if (strings != NULL) {

      char *mangled_name = nullptr, *offset_begin = nullptr, *offset_end = nullptr;

      // find parantheses and +address offset surrounding mangled name
      for (char *p = strings[i]; *p; ++p) {
        if (*p == '(') {
          mangled_name = p; 
        }
        else if (*p == '+') {
          offset_begin = p;
        }
        else if (*p == ')') {
          offset_end = p;
          break;
        }
      }

      // if the line could be processed, attempt to demangle the symbol
      if (mangled_name && offset_begin && offset_end && 
          mangled_name < offset_begin) {
            *mangled_name++ = '\0';
            *offset_begin++ = '\0';
            *offset_end++ = '\0';
            int status = 0;
            char * demangled_name = abi::__cxa_demangle(mangled_name, 0, 0, &status);

            if (demangled_name != nullptr) {
              if (status == 0) {
                ss << stack_frames[i];
                btstr +=  strings[i] +
                  std::string("() [") +
                  ss.str() +
                  std::string("] ") +
                  demangled_name +
                  std::string("\n");
              }
              else {
                btstr += strings[i] +
                  std::string("\n");
              }
              TRI_SystemFree(demangled_name);
            }
      }
      else {
        btstr += strings[i] +
          std::string("\n");
      }
    }
    else {
      ss << stack_frames[i];
      btstr += ss.str() +
        std::string("\n");
    }
  }
  if (strings != NULL) {
    TRI_SystemFree(strings);  
  }
#endif
}

#ifndef TRI_ASSERT
#define TRI_ASSERT(expr) { if (!(expr)) _backtrace(); assert(expr);}
#define TRI_ASSERT_EXPENSIVE(expr) {if (!(expr)) _backtrace(); assert(expr);}
#endif

#else

#ifndef TRI_ASSERT
#define TRI_ASSERT(expr) (static_cast<void>(0))
#define TRI_ASSERT_EXPENSIVE(expr) (static_cast<void>(0))
#endif

#endif

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

// -----------------------------------------------------------------------------
// --SECTIONS--                                               triagens namespace
// -----------------------------------------------------------------------------

namespace triagens {
  using namespace std;

  typedef TRI_blob_t blob_t;
  typedef TRI_datetime_t datetime_t;
  typedef TRI_date_t date_t;
  typedef TRI_seconds_t seconds_t;
  typedef TRI_msec_t msec_t;
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
