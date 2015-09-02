////////////////////////////////////////////////////////////////////////////////
/// @brief debugging helpers
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Basics/logging.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/WriteLocker.h"

#ifdef TRI_ENABLE_MAINTAINER_MODE
#if HAVE_BACKTRACE
#ifdef _WIN32
#include <DbgHelp.h>
#else
#include <execinfo.h>
#include <cxxabi.h>
#endif
#endif
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief a global string containing the currently registered failure points
/// the string is a comma-separated list of point names
////////////////////////////////////////////////////////////////////////////////

static char* FailurePoints = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief a read-write lock for thread-safe access to the failure-points list
////////////////////////////////////////////////////////////////////////////////

triagens::basics::ReadWriteLock FailurePointsLock;

#ifdef TRI_ENABLE_FAILURE_TESTS

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief make a delimited value from a string, so we can unambigiously
/// search for it (e.g. searching for just "foo" would find "foo" and "foobar",
/// so we'll be putting the value inside some delimiter: ",foo,")
////////////////////////////////////////////////////////////////////////////////

static char* MakeValue (char const* value) {
  if (value == nullptr || strlen(value) == 0) {
    return nullptr;
  }

  char* delimited = static_cast<char*>(TRI_Allocate(TRI_CORE_MEM_ZONE, strlen(value) + 3, false));

  if (delimited != nullptr) {
    memcpy(delimited + 1, value, strlen(value));
    delimited[0] = ',';
    delimited[strlen(value) + 1] = ',';
    delimited[strlen(value) + 2] = '\0';
  }

  return delimited;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief cause a segmentation violation
/// this is used for crash and recovery tests
////////////////////////////////////////////////////////////////////////////////

void TRI_SegfaultDebugging (char const* message) {
  LOG_WARNING("%s: summon Baal!", message);
  // make sure the latest log messages are flushed
  TRI_ShutdownLogging(true);

  // and now crash
#ifndef __APPLE__
  // on MacOS, the following statement makes the server hang but not crash
  *((char*) -1) = '!';
#endif

  // ensure the process is terminated
  abort();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether we should fail at a specific failure point
////////////////////////////////////////////////////////////////////////////////

bool TRI_ShouldFailDebugging (char const* value) {
  char* found = nullptr;

  // try without the lock first (to speed things up)
  if (FailurePoints == nullptr) {
    return false;
  }

  READ_LOCKER(FailurePointsLock);

  if (FailurePoints != nullptr) {
    char* checkValue = MakeValue(value);

    if (checkValue != nullptr) {
      found = strstr(FailurePoints, checkValue);
      TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
    }
  }

  return (found != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a failure point
////////////////////////////////////////////////////////////////////////////////

void TRI_AddFailurePointDebugging (char const* value) {
  char* found;
  char* checkValue = MakeValue(value);

  if (checkValue == nullptr) {
    return;
  }

  WRITE_LOCKER(FailurePointsLock);

  if (FailurePoints == nullptr) {
    found = nullptr;
  }
  else {
    found = strstr(FailurePoints, checkValue);
  }

  if (found == nullptr) {
    // not yet found. so add it
    char* copy;

    LOG_WARNING("activating intentional failure point '%s'. the server will misbehave!", value);
    size_t n = strlen(checkValue);

    if (FailurePoints == nullptr) {
      copy = static_cast<char*>(TRI_Allocate(TRI_CORE_MEM_ZONE, n + 1, false));

      if (copy == nullptr) {
        TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
        return;
      }

      memcpy(copy, checkValue, n);
      copy[n] = '\0';
    }
    else {
      copy = static_cast<char*>(TRI_Allocate(TRI_CORE_MEM_ZONE, n + strlen(FailurePoints), false));

      if (copy == nullptr) {
        TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
        return;
      }

      memcpy(copy, FailurePoints, strlen(FailurePoints));
      memcpy(copy + strlen(FailurePoints) - 1, checkValue, n);
      copy[strlen(FailurePoints) + n - 1] = '\0';
    }

    FailurePoints = copy;
  }

  TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a failure points
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveFailurePointDebugging (char const* value) {
  WRITE_LOCKER(FailurePointsLock);

  if (FailurePoints == nullptr) {
    return;
  }

  char* checkValue = MakeValue(value);

  if (checkValue != nullptr) {
    char* found;
    char* copy;
    size_t n;

    found = strstr(FailurePoints, checkValue);

    if (found == nullptr) {
      TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
      return;
    }

    if (strlen(FailurePoints) - strlen(checkValue) <= 2) {
      TRI_Free(TRI_CORE_MEM_ZONE, FailurePoints);
      FailurePoints = nullptr;

      TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
      return;
    }

    copy = static_cast<char*>(TRI_Allocate(TRI_CORE_MEM_ZONE, strlen(FailurePoints) - strlen(checkValue) + 2, false));

    if (copy == nullptr) {
      TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
      return;
    }

    // copy start of string
    n = found - FailurePoints;
    memcpy(copy, FailurePoints, n);

    // copy remainder of string
    memcpy(copy + n, found + strlen(checkValue) - 1, strlen(FailurePoints) - strlen(checkValue) - n + 1);

    copy[strlen(FailurePoints) - strlen(checkValue) + 1] = '\0';
    TRI_Free(TRI_CORE_MEM_ZONE, FailurePoints);
    FailurePoints = copy;

    TRI_Free(TRI_CORE_MEM_ZONE, checkValue);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clear all failure points
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearFailurePointsDebugging () {
  WRITE_LOCKER(FailurePointsLock);

  if (FailurePoints != nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, FailurePoints);
  }

  FailurePoints = nullptr;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the debugging
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseDebugging () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown the debugging
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownDebugging () {
  if (FailurePoints != nullptr) {
    TRI_Free(TRI_CORE_MEM_ZONE, FailurePoints);
  }

  FailurePoints = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief appends a backtrace to the string provided
////////////////////////////////////////////////////////////////////////////////

void TRI_GetBacktrace (std::string& btstr) {
#if HAVE_BACKTRACE
#ifdef _WIN32
  void         * stack[100];
  unsigned short frames;
  SYMBOL_INFO  * symbol;
  HANDLE         process;

  process = GetCurrentProcess();

  SymInitialize(process, nullptr, true);

  frames               = CaptureStackBackTrace(0, 100, stack, nullptr);
  symbol               = (SYMBOL_INFO*) calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);

  if (symbol == nullptr) {
    // cannot allocate memory
    return;
  }

  symbol->MaxNameLen   = 255;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

  for (unsigned int i = 0; i < frames; i++) {
    char address[64];
    SymFromAddr(process, (DWORD64) stack[i], 0, symbol);

    snprintf(address, sizeof(address), "0x%0X", (unsigned int) symbol->Address);
    btstr += std::to_string(frames - i - 1) + std::string(": ") + symbol->Name + std::string(" [") + address + std::string("]\n");
  }

  TRI_SystemFree(symbol);

#else
  void* stack_frames[50];
  size_t size, i;
  char** strings;

  size = backtrace(stack_frames, sizeof(stack_frames) / sizeof(void*));
  strings = backtrace_symbols(stack_frames, size);
  for (i = 0; i < size; i++) {
    std::stringstream ss;
    if (strings != nullptr) {
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
      // TODO: osx demangling works a little bit different. 
      // http://oroboro.com/stack-trace-on-crash/ 
      // says it should look like that:
      //#ifdef DARWIN
      //      // OSX style stack trace
      //      for ( char *p = symbollist[i]; *p; ++p )
      //      {
      //         if (( *p == '_' ) && ( *(p-1) == ' ' ))
      //            begin_name = p-1;
      //         else if ( *p == '+' )
      //            begin_offset = p-1;
      //      }
      //
      //      if ( begin_name && begin_offset && ( begin_name < begin_offset ))
      //      {
      //         *begin_name++ = '\0';
      //         *begin_offset++ = '\0';
      //
      //         // mangled name is now in [begin_name, begin_offset) and caller
      //         // offset in [begin_offset, end_offset). now apply
      //         // __cxa_demangle():
      //         int status;
      //         char* ret = abi::__cxa_demangle( begin_name, &funcname[0],
      //                                          &funcnamesize, &status );
      //         if ( status == 0 ) 
      //         {
      //            funcname = ret; // use possibly realloc()-ed string
      //            fprintf( out, "  %-30s %-40s %s\n",
      //                     symbollist[i], funcname, begin_offset );
      //         } else {
      //            // demangling failed. Output function name as a C function with
      //            // no arguments.
      //            fprintf( out, "  %-30s %-38s() %s\n",
      //                     symbollist[i], begin_name, begin_offset );
      //         }
      //
      //#else // !DARWIN - but is posix
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
  if (strings != nullptr) {
    TRI_SystemFree(strings);  
  }
#endif
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a backtrace on stderr
////////////////////////////////////////////////////////////////////////////////

void TRI_PrintBacktrace () {
#if HAVE_BACKTRACE
  std::string out;
  TRI_GetBacktrace(out);
  fprintf(stderr, "%s", out.c_str());
#endif
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
